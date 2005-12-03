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
#include "connection.h"
#include "handler.h"

static int should_exit = 0;

struct relay_list relays = {
    .next = 0,
};

static void usage(void)
     __attribute__ ((noreturn));

static void usage(void) {
    fprintf(stderr, "usage: uoproxy local:port server:port\n");
    exit(1);
}

static void exit_signal_handler(int sig) {
    (void)sig;
    should_exit = 1;
}

static void delete_all_connections(struct connection *head) {
    while (head != NULL) {
        struct connection *c = head;
        head = head->next;
        connection_delete(c);
    }
}

static void connections_pre_select(struct connection **cp, struct selectx *sx) {
    while (*cp != NULL) {
        struct connection *c = *cp;

        if (c->client != NULL &&
            !uo_client_alive(c->client)) {
            fprintf(stderr, "server disconnected\n");
            *cp = c->next;
            connection_delete(c);
            continue;
        }

        if (!uo_server_alive(c->server)) {
            fprintf(stderr, "client disconnected\n");
            *cp = c->next;
            connection_delete(c);
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
    packet_action_t action;

    if (c->client != NULL) {
        uo_client_post_select(c->client, sx);
        length = sizeof(buffer);
        while ((p = uo_client_receive(c->client, buffer, &length)) != NULL) {
            action = handle_packet(server_packet_bindings,
                                   c, p, length);
            switch (action) {
            case PA_ACCEPT:
                if (c->server != NULL)
                    uo_server_send(c->server, p, length);
                break;

            case PA_DROP:
                break;

            case PA_DISCONNECT:
                /* XXX */
                break;
            }

            length = sizeof(buffer);
        }
    }

    uo_server_post_select(c->server, sx);

    length = sizeof(buffer);
    while ((p = uo_server_receive(c->server, buffer, &length)) != NULL) {
        action = handle_packet(client_packet_bindings,
                               c, p, length);
        switch (action) {
        case PA_ACCEPT:
            if (c->client != NULL)
                uo_client_send(c->client, p, length);
            break;

        case PA_DROP:
            break;

        case PA_DISCONNECT:
            /* XXX */
            break;
        }

        length = sizeof(buffer);
    }
}

static void connections_post_select(struct connection *c, struct selectx *sx) {
    while (c != NULL) {
        connection_post_select(c, sx);
        c = c->next;
    }
}

static void run_server(uint32_t local_ip, uint16_t local_port,
                       uint32_t server_ip, uint16_t server_port) {
    int sockfd, ret;
    struct selectx sx;
    struct connection *connections_head = NULL;

    sockfd = setup_server_socket(local_ip, local_port);

    while (!should_exit) {
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
                    struct connection *c;

                    ret = connection_new(sockfd2,
                                         local_ip, local_port,
                                         server_ip, server_port,
                                         &c);
                    if (ret == 0) {
                        c->next = connections_head;
                        connections_head = c;
                    } else {
                        fprintf(stderr, "connection_new() failed: %s\n",
                                strerror(-ret));
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

    delete_all_connections(connections_head);
}

static void setup_signal_handlers(void) {
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = exit_signal_handler;

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
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

    /* set up */

    setup_signal_handlers();

    /* call main loop */
    run_server(local_ip, local_port, remote_ip, remote_port);

    return 0;
}
