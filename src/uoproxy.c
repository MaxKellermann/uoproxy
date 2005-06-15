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
};

static void usage(void)
     __attribute__ ((noreturn));

static void usage(void) {
    fprintf(stderr, "usage: uoproxy local:port server:port\n");
    exit(1);
}

static int getaddrinfo_helper(const char *host_and_port, int default_port,
                              const struct addrinfo *hints,
                              struct addrinfo **aip) {
    const char *colon, *host, *port;
    char buffer[256];

    colon = strchr(host_and_port, ':');
    if (colon == NULL) {
        snprintf(buffer, sizeof(buffer), "%d", default_port);

        host = host_and_port;
        port = buffer;
    } else {
        size_t len = colon - host_and_port;

        if (len >= sizeof(buffer)) {
            errno = ENAMETOOLONG;
            return EAI_SYSTEM;
        }

        memcpy(buffer, host_and_port, len);
        buffer[len] = 0;

        host = buffer;
        port = colon + 1;
    }

    if (strcmp(host, "*") == 0)
        host = "0.0.0.0";

    return getaddrinfo(host, port, hints, aip);
}

static void packet_from_client(struct connection *c,
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

    while (next + 3 <= packet->data + packet->length) {
        printf("from client: %02x %02x %02x\n",
               next[0], next[1], next[2]);

        p = next;

        length = packet_lengths[p[0]];
        if (length == 0)
            length = ntohs(*(uint16_t*)(p + 1));
        printf("length=%u\n", (unsigned)length);
        next = p + length;

        if (length == 0 || next > packet->data + packet->length) {
            fprintf(stderr, "malformed packet from client\n");
            return;
        }

        switch (p[0]) {
        case PCK_GameLogin:
            c->compressed = 1;
            break;
        }
    }
}

static void run_server(uint32_t local_ip, uint16_t local_port,
                       uint32_t server_ip, uint16_t server_port)
     __attribute__ ((noreturn));

static void packet_from_server(struct connection *c,
                               struct packet *packet) {
    unsigned char *p, *next;
    size_t length;

    (void)c;

    printf("packet from server: %u\n", (unsigned)packet->length);

    if (packet->length < 3)
        return;

    next = packet->data;

    while (next + 3 <= packet->data + packet->length) {
        printf("from server: %02x %02x %02x\n",
               next[0], next[1], next[2]);

        p = next;

        length = packet_lengths[p[0]];
        if (length == 0)
            length = ntohs(*(uint16_t*)(p + 1));
        printf("length=%u\n", (unsigned)length);
        next = p + length;

        if (length == 0 || next > packet->data + packet->length) {
            fprintf(stderr, "malformed packet from server\n");
            return;
        }

        switch (p[0]) {
            unsigned count, i, k;
            struct server_info *server_info;
            pid_t pid;

        case 0xa8: /* AccountLoginAck */
            if (length < 6 || p[3] != 0x5d)
                continue;

            count = ntohs(*(uint16_t*)(p + 4));
            printf("serverlist: %u servers\n", count);
            if (length != 6 + count * sizeof(*server_info))
                continue;

            server_info = (struct server_info*)(p + 6);
            for (i = 0; i < count; i++, server_info++) {
                k = ntohs(server_info->index);
                if (k != i)
                    break;

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
            pid = fork();
            if (pid < 0) {
                fprintf(stderr, "fork failed: %s", strerror(errno));
                continue;
            }

            if (pid == 0)
                run_server(c->local_ip,
                           *(uint16_t*)(p + 5),
                           *(uint32_t*)(p + 1),
                           *(uint16_t*)(p + 5));

            *(uint32_t*)(p + 1) = c->local_ip;

            break;
        }
    }
}

static void handle_connection(struct connection *c)
     __attribute__ ((noreturn));
static void handle_connection(struct connection *c) {
    int ret;
    fd_set rfds, wfds;
    struct packet packet1, packet2, packet3;
    ssize_t nbytes;

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
        if (ret < 0) {
            fprintf(stderr, "select failed: %s\n",
                    strerror(errno));
            exit(1);
        }

        assert(ret > 0);

        if (packet1.length == 0) {
            if (FD_ISSET(c->client_socket, &rfds)) {
                nbytes = recv(c->client_socket, packet1.data, sizeof(packet1.data), 0);
                if (nbytes <= 0) {
                    printf("client disconnected\n");
                    exit(0);
                }

                if (nbytes > 0) {
                    packet1.length = (size_t)nbytes;
                    packet_from_client(c, &packet1);
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
                            packet_from_server(c, &packet3);
                        }
                    } else {
                        packet_from_server(c, &packet2);
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
    }
}

static int setup_server_socket(uint32_t ip, uint16_t port) {
    int sockfd, ret, param;
    struct sockaddr_in sin;

    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "failed to create socket: %s\n",
                strerror(errno));
        exit(1);
    }

    param = 1;
    ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &param, sizeof(param));
    if (ret < 0) {
        fprintf(stderr, "setsockopt failed: %s\n",
                strerror(errno));
        exit(1);
    }

    sin.sin_family = AF_INET;
    sin.sin_port = port;
    sin.sin_addr.s_addr = ip;

    ret = bind(sockfd, (const struct sockaddr*)&sin, sizeof(sin));
    if (ret < 0) {
        fprintf(stderr, "failed to bind: %s\n",
                strerror(errno));
        exit(1);
    }

    ret = listen(sockfd, 4);
    if (ret < 0) {
        fprintf(stderr, "listen failed: %s\n",
                strerror(errno));
        exit(1);
    }

    return sockfd;
}

static int setup_client_socket(uint32_t ip, uint16_t port) {
    int sockfd, ret;
    struct sockaddr_in sin;

    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "failed to create socket: %s\n",
                strerror(errno));
        exit(1);
    }

    sin.sin_family = AF_INET;
    sin.sin_port = port;
    sin.sin_addr.s_addr = ip;

    ret = connect(sockfd, (struct sockaddr*)&sin, sizeof(sin));
    if (ret < 0) {
        fprintf(stderr, "connect failed: %s\n",
                strerror(errno));
        exit(1);
    }

    return sockfd;
}

static void run_server(uint32_t local_ip, uint16_t local_port,
                       uint32_t server_ip, uint16_t server_port) {
    int sockfd, sockfd2;
    pid_t pid;

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
