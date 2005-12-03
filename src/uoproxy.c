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
#include "client.h"

struct connection {
    struct connection *next;
    uint32_t seed;
    uint32_t local_ip, server_ip;
    uint16_t local_port;
    struct uo_client *client;
    struct sock_buff *server;
    int compressed;
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

        uo_client_send(c->client, p, 4);
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

        uo_client_send(c->client, p, packet_length);
        buffer_remove_head(buffer, packet_length);
    }
}

static void packet_from_server(struct connection *c,
                               unsigned char *p,
                               size_t length) {
    assert(length > 0);

    printf("packet from server: 0x%02x length=%zu\n", p[0], length);

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

static void delete_connection(struct connection *c) {
    sock_buff_dispose(c->server);
    uo_client_dispose(c->client);
    free(c);
}

static void connections_pre_select(struct connection **cp, struct selectx *sx) {
    while (*cp != NULL) {
        struct connection *c = *cp;

        if (!uo_client_alive(c->client)) {
            fprintf(stderr, "server disconnected\n");
            *cp = c->next;
            delete_connection(c);
            continue;
        }

        if (!sock_buff_alive(c->server)) {
            fprintf(stderr, "client disconnected\n");
            *cp = c->next;
            delete_connection(c);
            continue;
        }

        uo_client_pre_select(c->client, sx);
        sock_buff_pre_select(c->server, sx);

        cp = &c->next;
    }
}

static void connection_post_select(struct connection *c, struct selectx *sx) {
    unsigned char buffer[2048], *p;
    size_t length;

    uo_client_post_select(c->client, sx);
    sock_buff_post_select(c->server, sx);

    length = sizeof(buffer);
    while ((p = uo_client_receive(c->client, buffer, &length)) != NULL) {
        packet_from_server(c, p, length);

        if (c->compressed) {
            ssize_t nbytes;

            nbytes = uo_compress(buffer_tail(c->server->output),
                                 buffer_free(c->server->output),
                                 p, length);
            if (nbytes < 0) {
                fprintf(stderr, "uo_compress() failed\n");
                exit(3);
            }

            printf("compressed %zu bytes to %zd\n",
                   length, nbytes);

            buffer_expand(c->server->output, (size_t)nbytes);
        } else {
            buffer_append(c->server->output, p, length);
        }

        length = sizeof(buffer);
    }

    p = buffer_peek(c->server->input, &length);
    if (p != NULL)
        recv_from_client(c, c->server->input);
}

static void connections_post_select(struct connection *c, struct selectx *sx) {
    while (c != NULL) {
        connection_post_select(c, sx);
        c = c->next;
    }
}

static struct connection *create_connection(int server_socket,
                                            uint32_t local_ip, uint16_t local_port,
                                            uint32_t server_ip, uint16_t server_port) {
    struct connection *c;
    int ret;

    c = calloc(1, sizeof(*c));
    if (c == NULL) {
        fprintf(stderr, "out of memory\n");
        return NULL;
    }

    c->local_ip = local_ip;
    c->local_port = local_port;
    ret = sock_buff_create(server_socket, 4096, 65536, &c->server);
    if (ret != 0) {
        fprintf(stderr, "sock_buff_create() failed: %s\n",
                strerror(-ret));
        free(c);
        return NULL;
    }

    ret = uo_client_create(server_ip, server_port, &c->client);
    if (ret != 0) {
        fprintf(stderr, "sock_buff_create() failed: %s\n",
                strerror(-ret));
        sock_buff_dispose(c->server);
        free(c);
        return NULL;
    }

    return c;
}

static void run_server(uint32_t local_ip, uint16_t local_port,
                       uint32_t server_ip, uint16_t server_port)
     __attribute__ ((noreturn));

static void run_server(uint32_t local_ip, uint16_t local_port,
                       uint32_t server_ip, uint16_t server_port) {
    int sockfd;
    struct sigaction sa;
    int ret;
    struct selectx sx;
    struct connection *connections_head = NULL;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;

    sigaction(SIGCHLD, &sa, NULL);

    sockfd = setup_server_socket(local_ip, local_port);

    while (1) {
        selectx_clear(&sx);
        selectx_add_read(&sx, sockfd);
        connections_pre_select(&connections_head, &sx);

        ret = selectx(&sx, NULL);
        assert(ret != 0);
        if (ret > 0) {
            if (FD_ISSET(sockfd, &sx.readfds)) {
                int sockfd2;
                struct sockaddr addr;
                socklen_t addrlen = sizeof(addr);

                sockfd2 = accept(sockfd, &addr, &addrlen);
                if (sockfd2 >= 0) {
                    struct connection *c = create_connection
                        (sockfd2, local_ip, local_port,
                         server_ip, server_port);

                    if (c != NULL) {
                        c->next = connections_head;
                        connections_head = c;
                    } else {
                        close(sockfd2);
                    }
                } else if (errno != EINTR) {
                    fprintf(stderr, "accept failed: %s\n",
                            strerror(errno));
                    exit(1);
                }
            }

            connections_post_select(connections_head, &sx);
        } else if (errno != EINTR) {
            fprintf(stderr, "select failed: %s\n",
                    strerror(errno));
            exit(1);
        }
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
