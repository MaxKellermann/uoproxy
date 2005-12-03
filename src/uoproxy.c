/*
 * uoproxy
 * $Id$
 *
 * (c) 2005 Max Kellermann <max@duempel.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>

#include "compression.h"
#include "packets.h"
#include "netutil.h"
#include "ioutil.h"
#include "buffer.h"
#include "sockbuff.h"

struct connection {
    uint32_t seed;
    uint32_t local_ip, server_ip;
    uint16_t local_port;
    struct sock_buff *client, *server;
    int compressed;
    struct uo_decompression decompression;
    struct buffer *decompressed_buffer;
};

struct server_info {
    uint16_t index;
    char name[32];
    char full;
    unsigned char timezone;
    uint32_t address;
} __attribute__ ((packed));

static void usage(void)
     __attribute__ ((noreturn));

static void usage(void) {
    fprintf(stderr, "usage: uoproxy local:port server:port\n");
    exit(1);
}

static void signal_handler(int sig) {
    (void)sig;
    /* do nothing, just interrupt system calls */
}

static void packet_from_client(struct connection *c,
                               const unsigned char *p,
                               size_t length) {
    assert(length > 0);

    switch (p[0]) {
        const struct uo_packet_login *packet_login;

    case PCK_AccountLogin:
    case PCK_AccountLogin2:
        packet_login = (const struct uo_packet_login*)p;

        printf("account_login: username=%s password=%s\n",
               packet_login->username, packet_login->password);

        break;

    case PCK_GameLogin:
        c->compressed = 1;
        break;
    }
}

static void recv_from_client(struct connection *c,
                             struct buffer *buffer) {
    unsigned char *p;
    size_t length, packet_length;

    if (c->seed == 0) {
        p = buffer_peek(buffer, &length);
        if (p == NULL || length < 4)
            return;

        c->seed = ntohl(*(uint32_t*)p);
        if (c->seed == 0) {
            fprintf(stderr, "zero seed from client\n");
            exit(3);
        }

        printf("seed=%08x\n", c->seed);

        buffer_append(c->client->output, p, 4);
        buffer_remove_head(buffer, 4);
    }

    while ((p = buffer_peek(buffer, &length)) != NULL) {
        printf("from client: %02x %02x %02x\n",
               p[0], p[1], p[2]);

        packet_length = packet_lengths[p[0]];
        if (packet_length == 0) {
            if (length < 3)
                return;

            packet_length = ntohs(*(uint16_t*)(p + 1));
            if (packet_length == 0) {
                fprintf(stderr, "malformed packet from client\n");
                exit(3);
            }
        }

        printf("packet_length=%zu\n", packet_length);
        if (packet_length > length)
            return;

        packet_from_client(c, p, packet_length);

        buffer_append(c->client->output, p, packet_length);
        buffer_remove_head(buffer, packet_length);
    }
}

static void packet_from_server(struct connection *c,
                               unsigned char *p,
                               size_t length) {
    assert(length > 0);

    switch (p[0]) {
        unsigned count, i, k;
        struct server_info *server_info;

    case 0xa8: /* AccountLoginAck */
        if (length < 6 || p[3] != 0x5d)
            return;

        count = ntohs(*(uint16_t*)(p + 4));
        printf("serverlist: %u servers\n", count);
        if (length != 6 + count * sizeof(*server_info))
            return;

        server_info = (struct server_info*)(p + 6);
        for (i = 0; i < count; i++, server_info++) {
            k = ntohs(server_info->index);
            if (k != i)
                return;

            printf("server %u: name=%s address=0x%08x\n",
                   ntohs(server_info->index),
                   server_info->name,
                   ntohl(server_info->address));
        }
        break;

    case 0x8c: /* PlayServerAck */
        /* this packet tells the UO client where to connect; what
           we do here is replace the server IP with our own one */

        printf("play_ack: address=0x%08x port=%u\n",
               ntohl(*(uint32_t*)(p + 1)),
               ntohs(*(uint16_t*)(p + 5)));

        *(uint32_t*)(p + 1) = c->local_ip;
        *(uint16_t*)(p + 5) = c->local_port;

        break;
    }
}

static void recv_from_server(struct connection *c,
                             struct buffer *buffer) {
    unsigned char *p;
    size_t length, packet_length;

    (void)c;

    while ((p = buffer_peek(buffer, &length)) != NULL) {
        printf("from server: %02x %02x %02x\n",
               p[0], p[1], p[2]);

        packet_length = packet_lengths[p[0]];
        if (packet_length == 0) {
            if (length < 3)
                return;

            packet_length = ntohs(*(uint16_t*)(p + 1));
            if (packet_length == 0) {
                fprintf(stderr, "malformed packet from server\n");
                exit(3);
            }
        }

        printf("packet_length=%zu\n", packet_length);
        if (packet_length > length)
            return;

        packet_from_server(c, p, packet_length);

        if (c->compressed) {
            ssize_t nbytes;

            nbytes = uo_compress(buffer_tail(c->server->output),
                                 buffer_free(c->server->output),
                                 p, packet_length);
            if (nbytes < 0) {
                fprintf(stderr, "uo_compress() failed\n");
                exit(3);
            }

            printf("compressed %zu bytes to %zd\n",
                   packet_length, nbytes);

            buffer_expand(c->server->output, (size_t)nbytes);
        } else {
            buffer_append(c->server->output, p, packet_length);
        }

        buffer_remove_head(buffer, packet_length);
    }
}

static void handle_connection(struct connection *c)
     __attribute__ ((noreturn));
static void handle_connection(struct connection *c) {
    int ret;
    struct selectx sx;

    printf("entering handle_connection\n");

    while (1) {
        /* select() all file handles */

        selectx_clear(&sx);

        sock_buff_pre_select(c->client, &sx);
        sock_buff_pre_select(c->server, &sx);

        ret = selectx(&sx, NULL);
        assert(ret != 0);
        if (ret > 0) {
            const unsigned char *p;
            size_t length;
            ssize_t nbytes;

            sock_buff_post_select(c->client, &sx);
            sock_buff_post_select(c->server, &sx);

            p = buffer_peek(c->client->input, &length);
            if (p != NULL) {
                if (c->compressed) {
                    nbytes = uo_decompress(&c->decompression,
                                           buffer_tail(c->decompressed_buffer),
                                           buffer_free(c->decompressed_buffer),
                                           p, length);
                    if (nbytes < 0) {
                        fprintf(stderr, "decompression failed\n");
                        exit(3);
                    }

                    buffer_remove_head(c->client->input, length);
                    buffer_expand(c->decompressed_buffer, (size_t)nbytes);

                    printf("decompressed %zu bytes to %zd\n",
                           length, nbytes);

                    recv_from_server(c, c->decompressed_buffer);
                } else {
                    recv_from_server(c, c->client->input);
                }
            }

            p = buffer_peek(c->server->input, &length);
            if (p != NULL)
                recv_from_client(c, c->server->input);
        } else if (errno != EINTR) {
            fprintf(stderr, "select failed: %s\n",
                    strerror(errno));
            exit(1);
        }

        /*
        do {
            int status;
            pid_t pid;

            pid = waitpid(-1, &status, WNOHANG);
        } while (pid > 0);
        */
    }
}

static void run_server(uint32_t local_ip, uint16_t local_port,
                       uint32_t server_ip, uint16_t server_port)
     __attribute__ ((noreturn));

static void run_server(uint32_t local_ip, uint16_t local_port,
                       uint32_t server_ip, uint16_t server_port) {
    int sockfd, sockfd2;
    pid_t pid;
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;

    sigaction(SIGCHLD, &sa, NULL);

    sockfd = setup_server_socket(local_ip, local_port);

    while (1) {
        struct sockaddr addr;
        socklen_t addrlen = sizeof(addr);

        sockfd2 = accept(sockfd, &addr, &addrlen);
        if (sockfd2 >= 0) {
            pid = fork();
            if (pid < 0) {
                fprintf(stderr, "fork failed: %s\n",
                        strerror(errno));
                exit(1);
            }

            if (pid == 0) {
                int ret;
                struct connection c;

                close(sockfd);

                memset(&c, 0, sizeof(c));

                c.local_ip = local_ip;
                c.local_port = local_port;
                ret = sock_buff_create(sockfd2, 4096, 65536, &c.server);
                if (ret != 0) {
                    fprintf(stderr, "sock_buff_create() failed: %s\n",
                            strerror(-ret));
                    exit(2);
                }

                ret = setup_client_socket(server_ip, server_port);
                ret = sock_buff_create(ret, 4096, 65536, &c.client);
                if (ret != 0) {
                    fprintf(stderr, "sock_buff_create() failed: %s\n",
                            strerror(-ret));
                    exit(2);
                }

                uo_decompression_init(&c.decompression);
                c.decompressed_buffer = buffer_new(65536);
                if (c.decompressed_buffer == NULL) {
                    fprintf(stderr, "out of memory\n");
                    exit(2);
                }

                handle_connection(&c);
            }

            close(sockfd2);
        } else if (errno != EINTR) {
            fprintf(stderr, "accept failed: %s\n",
                    strerror(errno));
            exit(1);
        }

        do {
            int status;

            pid = waitpid(-1, &status, WNOHANG);
        } while (pid > 0);
    }
}

int main(int argc, char **argv) {
    int ret;
    struct addrinfo hints, *ai;
    uint32_t local_ip, remote_ip;
    uint16_t local_port, remote_port;
    const struct sockaddr_in *sin;

    if (argc != 3)
        usage();

    /* parse local address */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;

    ret = getaddrinfo_helper(argv[1], 2593, &hints, &ai);
    if (ret != 0 || ai == NULL) {
        fprintf(stderr, "failed to resolve local hostname\n");
        exit(1);
    }

    if (ai->ai_family != PF_INET) {
        fprintf(stderr, "only IPv4 supported\n");
        exit(1);
    }

    sin = (const struct sockaddr_in*)ai->ai_addr;
    local_port = sin->sin_port;
    local_ip = sin->sin_addr.s_addr;

    freeaddrinfo(ai);

    /* parse server address */
    ret = getaddrinfo_helper(argv[2], 2593, &hints, &ai);
    if (ret != 0 || ai == NULL) {
        fprintf(stderr, "failed to resolve server hostname\n");
        exit(1);
    }

    if (ai->ai_family != PF_INET) {
        fprintf(stderr, "only IPv4 supported\n");
        exit(1);
    }

    sin = (const struct sockaddr_in*)ai->ai_addr;
    remote_port = sin->sin_port;
    remote_ip = sin->sin_addr.s_addr;

    freeaddrinfo(ai);

    /* call main loop */
    run_server(local_ip, local_port, remote_ip, remote_port);
}
