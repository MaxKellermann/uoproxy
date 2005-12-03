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

struct connection {
    uint32_t seed;
    uint32_t local_ip, server_ip;
    int client_socket, server_socket;
    int compressed;
};

struct packet {
    size_t length;
    unsigned char data[65536];
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

static void handle_incoming(int sockfd,
                            uint32_t local_ip, uint16_t local_port,
                            uint32_t server_ip, uint16_t server_port)
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
                             struct packet *packet) {
    unsigned char *p, *next;
    size_t length;

    printf("packet from client: %u\n", (unsigned)packet->length);

    next = packet->data;

    if (c->seed == 0) {
        if (packet->length < 4) {
            fprintf(stderr, "malformed seed packet from client\n");
            return;
        }

        c->seed = ntohl(*(uint32_t*)packet->data);
        if (c->seed == 0) {
            fprintf(stderr, "zero seed from client\n");
            return;
        }

        printf("seed=%08x\n", c->seed);

        next += 4;
    }

    if (packet->length < 3)
        return;

    while (next < packet->data + packet->length) {
        printf("from client: %02x %02x %02x\n",
               next[0], next[1], next[2]);

        p = next;

        length = packet_lengths[p[0]];
        if (length == 0 && next + 3 <= packet->data + packet->length)
            length = ntohs(*(uint16_t*)(p + 1));
        printf("length=%u\n", (unsigned)length);
        next = p + length;

        if (length == 0 || next > packet->data + packet->length) {
            fprintf(stderr, "malformed packet from client\n");
            return;
        }

        packet_from_client(c, p, length);
    }
}

static void packet_from_server(struct connection *c,
                               unsigned char *p,
                               size_t length) {
    assert(length > 0);

    switch (p[0]) {
        unsigned count, i, k;
        struct server_info *server_info;
        uint16_t local_port;
        int sockfd;
        pid_t pid;

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
           we do here is replace the server IP with our own one,
           and accept incoming connections with a new proxy
           process */

        printf("play_ack: address=0x%08x port=%u\n",
               ntohl(*(uint32_t*)(p + 1)),
               ntohs(*(uint16_t*)(p + 5)));

        local_port = ntohs(*(uint16_t*)(p + 5));
        sockfd = setup_server_socket_random_port(c->local_ip,
                                                 &local_port);

        pid = fork();
        if (pid < 0) {
            fprintf(stderr, "fork failed: %s", strerror(errno));
            return;
        }

        if (pid == 0)
            handle_incoming(sockfd,
                            c->local_ip,
                            local_port,
                            *(uint32_t*)(p + 1),
                            *(uint16_t*)(p + 5));

        *(uint32_t*)(p + 1) = c->local_ip;
        *(uint16_t*)(p + 5) = htons(local_port);

        break;
    }
}

static void recv_from_server(struct connection *c,
                             struct packet *packet) {
    unsigned char *p, *next;
    size_t length;

    (void)c;

    printf("packet from server: %u\n", (unsigned)packet->length);

    if (packet->length < 3)
        return;

    next = packet->data;

    while (next < packet->data + packet->length) {
        printf("from server: %02x %02x %02x\n",
               next[0], next[1], next[2]);

        p = next;

        length = packet_lengths[p[0]];
        if (length == 0 && next + 3 <= packet->data + packet->length)
            length = ntohs(*(uint16_t*)(p + 1));
        printf("length=%u\n", (unsigned)length);
        next = p + length;

        if (length == 0 || next > packet->data + packet->length) {
            fprintf(stderr, "malformed packet from server\n");
            return;
        }

        packet_from_server(c, p, length);
    }
}

static void handle_connection(struct connection *c)
     __attribute__ ((noreturn));
static void handle_connection(struct connection *c) {
    int ret;
    fd_set rfds, wfds;
    struct packet packet1, packet2, packet3;
    ssize_t nbytes;
    pid_t pid;

    printf("entering handle_connection\n");

    packet1.length = 0;
    packet2.length = 0;

    while (1) {
        /* select() all file handles */

        FD_ZERO(&rfds);
        FD_ZERO(&wfds);

        ret = 0;

        if (packet1.length == 0) {
            FD_SET(c->client_socket, &rfds);
            if (c->client_socket > ret)
                ret = c->client_socket;
        } else {
            FD_SET(c->server_socket, &wfds);
            if (c->server_socket > ret)
                ret = c->server_socket;
        }

        if (packet2.length == 0) {
            FD_SET(c->server_socket, &rfds);
            if (c->server_socket > ret)
                ret = c->server_socket;
        } else {
            FD_SET(c->client_socket, &wfds);
            if (c->client_socket > ret)
                ret = c->client_socket;
        }

        ret = select(ret + 1, &rfds, &wfds, NULL, NULL);
        assert(ret != 0);
        if (ret > 0) {
            if (packet1.length == 0) {
                if (FD_ISSET(c->client_socket, &rfds)) {
                    nbytes = recv(c->client_socket, packet1.data, sizeof(packet1.data), 0);
                    if (nbytes <= 0) {
                        printf("client disconnected\n");
                        exit(0);
                    }

                    if (nbytes > 0) {
                        packet1.length = (size_t)nbytes;
                        recv_from_client(c, &packet1);
                    }
                }
            } else {
                if (FD_ISSET(c->server_socket, &wfds)) {
                    nbytes = send(c->server_socket, packet1.data, packet1.length, 0);
                    if (nbytes < 0) {
                        fprintf(stderr, "failed to write: %s\n",
                                strerror(errno));
                        exit(1);
                    }

                    if ((size_t)nbytes < packet1.length) {
                        fprintf(stderr, "short write\n");
                        exit(1);
                    }

                    packet1.length = 0;
                }
            }

            if (packet2.length == 0) {
                if (FD_ISSET(c->server_socket, &rfds)) {
                    nbytes = recv(c->server_socket, packet2.data, sizeof(packet2.data), 0);
                    if (nbytes <= 0) {
                        printf("server disconnected\n");
                        exit(0);
                    }

                    if (nbytes > 0) {
                        packet2.length = (size_t)nbytes;

                        if (c->compressed) {
                            nbytes = uo_decompress(packet3.data, sizeof(packet3.data),
                                                   packet2.data, packet2.length);
                            fprintf(stderr, "decompressed %lu bytes to %ld\n",
                                    (unsigned long)packet2.length,
                                    (long)nbytes);
                            if (nbytes < 0) {
                                fprintf(stderr, "decompress failed\n");
                            } else {
                                packet3.length = (size_t)nbytes;
                                recv_from_server(c, &packet3);
                            }
                        } else {
                            recv_from_server(c, &packet2);
                        }
                    }
                }
            } else {
                if (FD_ISSET(c->client_socket, &wfds)) {
                    nbytes = send(c->client_socket, packet2.data, packet2.length, 0);
                    if (nbytes < 0) {
                        fprintf(stderr, "failed to write: %s\n",
                                strerror(errno));
                        exit(1);
                    }

                    if ((size_t)nbytes < packet2.length) {
                        fprintf(stderr, "short write\n");
                        exit(1);
                    }

                    packet2.length = 0;
                }
            }
        } else if (errno != EINTR) {
            fprintf(stderr, "select failed: %s\n",
                    strerror(errno));
            exit(1);
        }

        do {
            int status;

            pid = waitpid(-1, &status, WNOHANG);
        } while (pid > 0);
    }
}

static void handle_incoming(int sockfd,
                            uint32_t local_ip, uint16_t local_port,
                            uint32_t server_ip, uint16_t server_port) {
    int ret;
    fd_set rfds;
    struct timeval tv;
    struct sockaddr addr;
    socklen_t addrlen = sizeof(addr);
    struct connection c;

    (void)local_port;

    tv.tv_sec = 10;
    tv.tv_usec = 0;

    FD_ZERO(&rfds);
    FD_SET(sockfd, &rfds);

    ret = select(sockfd + 1, &rfds, NULL, NULL, &tv);
    if (ret < 0) {
        fprintf(stderr, "select failed: %s\n", strerror(errno));
        exit(1);
    }

    if (ret <= 0) {
        fprintf(stderr, "timeout\n");
        exit(1);
    }

    ret = accept(sockfd, &addr, &addrlen);
    if (ret < 0) {
        fprintf(stderr, "accept failed: %s\n",
                strerror(errno));
        exit(1);
    }

    close(sockfd);

    memset(&c, 0, sizeof(c));

    c.local_ip = local_ip;
    c.client_socket = ret;
    c.server_socket = setup_client_socket(server_ip, server_port);

    handle_connection(&c);
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
                struct connection c;

                close(sockfd);

                memset(&c, 0, sizeof(c));

                c.local_ip = local_ip;
                c.client_socket = sockfd2;
                c.server_socket = setup_client_socket(server_ip, server_port);

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
