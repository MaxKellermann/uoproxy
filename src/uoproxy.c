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

#include "packets.h"
#include "netutil.h"
#include "ioutil.h"
#include "client.h"
#include "server.h"
#include "relay.h"

static struct relay_list relays = {
    .next = 0,
};

struct connection {
    struct connection *next;
    uint32_t local_ip, server_ip;
    uint16_t local_port, server_port;
    struct uo_client *client;
    struct uo_server *server;
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

static void packet_from_client_account_login(struct connection *c,
                                             const struct uo_packet_login *p) {
    int ret;

    printf("account_login: username=%s password=%s\n",
           p->username, p->password);

    if (c->client != NULL) {
        fprintf(stderr, "already logged in\n");
        return;
    }

    ret = uo_client_create(c->server_ip, c->server_port,
                           uo_server_seed(c->server),
                           &c->client);
    if (ret != 0) {
        struct uo_packet_login_bad response;

        fprintf(stderr, "uo_client_create() failed: %s\n",
                strerror(-ret));

        response.cmd = PCK_LogBad;
        response.reason = 0x02; /* blocked */

        uo_server_send(c->server, (const unsigned char*)&response,
                       sizeof(response));
        return;
    }
}

static void packet_from_client_game_login(struct connection *c,
                                          const struct uo_packet_game_login *p) {
    int ret;
    const struct relay *relay;

    c->compressed = 1;

    printf("game_login: username=%s password=%s\n",
           p->username, p->password);

    if (c->client != NULL) {
        fprintf(stderr, "already logged in\n");
        return;
    }

    relay = relay_find(&relays, p->auth_id);
    if (relay == NULL) {
        fprintf(stderr, "invalid or expired auth_id: 0x%08x\n",
                p->auth_id);
        return;
    }

    c->server_ip = relay->server_ip;
    c->server_port = relay->server_port;

    ret = uo_client_create(c->server_ip, c->server_port,
                           uo_server_seed(c->server),
                           &c->client);
    if (ret != 0) {
        fprintf(stderr, "uo_client_create() failed: %s\n",
                strerror(-ret));
        return;
    }
}

static void packet_from_client(struct connection *c,
                               const unsigned char *p,
                               size_t length) {
    assert(length > 0);

    switch (p[0]) {
    case PCK_AccountLogin:
    case PCK_AccountLogin2:
        packet_from_client_account_login(c, (const struct uo_packet_login*)p);
        break;

    case PCK_GameLogin:
        packet_from_client_game_login(c, (const struct uo_packet_game_login*)p);
        break;
    }
}

static void packet_from_server_relay(struct connection *c,
                                     struct uo_packet_relay *p) {
    /* this packet tells the UO client where to connect; what
       we do here is replace the server IP with our own one */
    struct relay relay;

    printf("play_ack: address=0x%08x port=%u\n",
           ntohl(p->ip), ntohs(p->port));

    /* remember the original IP/port */
    relay = (struct relay){
        .auth_id = p->auth_id,
        .server_ip = p->ip,
        .server_port = p->port,
    };

    relay_add(&relays, &relay);

    /* now overwrite the packet */
    p->ip = c->local_ip;
    p->port = c->local_port;
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

    case PCK_Relay:
        packet_from_server_relay(c, (struct uo_packet_relay*)p);
        break;
    }
}

static void delete_connection(struct connection *c) {
    uo_server_dispose(c->server);
    if (c->client != NULL)
        uo_client_dispose(c->client);
    free(c);
}

static void connections_pre_select(struct connection **cp, struct selectx *sx) {
    while (*cp != NULL) {
        struct connection *c = *cp;

        if (c->client != NULL &&
            !uo_client_alive(c->client)) {
            fprintf(stderr, "server disconnected\n");
            *cp = c->next;
            delete_connection(c);
            continue;
        }

        if (!uo_server_alive(c->server)) {
            fprintf(stderr, "client disconnected\n");
            *cp = c->next;
            delete_connection(c);
            continue;
        }

        if (c->client != NULL)
            uo_client_pre_select(c->client, sx);
        uo_server_pre_select(c->server, sx);

        cp = &c->next;
    }
}

static void connection_post_select(struct connection *c, struct selectx *sx) {
    unsigned char buffer[2048], *p;
    size_t length;

    if (c->client != NULL) {
        uo_client_post_select(c->client, sx);
        length = sizeof(buffer);
        while ((p = uo_client_receive(c->client, buffer, &length)) != NULL) {
            packet_from_server(c, p, length);
            uo_server_send(c->server, p, length);

            length = sizeof(buffer);
        }
    }

    uo_server_post_select(c->server, sx);

    length = sizeof(buffer);
    while ((p = uo_server_receive(c->server, buffer, &length)) != NULL) {
        packet_from_client(c, p, length);
        if (c->client != NULL)
            uo_client_send(c->client, p, length);

        length = sizeof(buffer);
    }
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
    ret = uo_server_create(server_socket, &c->server);
    if (ret != 0) {
        fprintf(stderr, "sock_buff_create() failed: %s\n",
                strerror(-ret));
        close(server_socket);
        free(c);
        return NULL;
    }

    c->server_ip = server_ip;
    c->server_port = server_port;

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
