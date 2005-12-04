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

#include "netutil.h"
#include "ioutil.h"
#include "relay.h"
#include "connection.h"

static int should_exit = 0;

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

static void instance_pre_select(struct instance *instance,
                                struct selectx *sx) {
    struct connection **cp = &instance->connections_head;

    while (*cp != NULL) {
        struct connection *c = *cp;

        connection_pre_select(c, sx);

        if (c->invalid) {
            *cp = c->next;
            connection_delete(c);
        } else {
            cp = &c->next;
        }
    }
}

static void instance_post_select(struct instance *instance,
                                 struct selectx *sx) {
    struct connection *c;

    for (c = instance->connections_head; c != NULL; c = c->next)
        connection_post_select(c, sx);
}

static void instance_idle(struct instance *instance) {
    struct connection *c;

    for (c = instance->connections_head; c != NULL; c = c->next)
        connection_idle(c);
}

static void run_server(struct instance *instance,
                       uint32_t local_ip, uint16_t local_port) {
    int sockfd, ret;
    struct selectx sx;
    struct timeval tv = {
        .tv_sec = 30,
        .tv_usec = 0,
    };

    sockfd = setup_server_socket(local_ip, local_port);

    while (!should_exit) {
        selectx_clear(&sx);
        selectx_add_read(&sx, sockfd);
        instance_pre_select(instance, &sx);

        ret = selectx(&sx, &tv);
        if (ret == 0) {
            instance_idle(instance);
            tv.tv_sec = 30;
        } else if (ret > 0) {
            if (FD_ISSET(sockfd, &sx.readfds)) {
                int sockfd2;
                struct sockaddr addr;
                socklen_t addrlen = sizeof(addr);

                sockfd2 = accept(sockfd, &addr, &addrlen);
                if (sockfd2 >= 0) {
                    struct connection *c;

                    ret = connection_new(instance,
                                         sockfd2,
                                         &c);
                    if (ret == 0) {
                        c->next = instance->connections_head;
                        instance->connections_head = c;
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

            instance_post_select(instance, &sx);
        } else if (errno != EINTR) {
            fprintf(stderr, "select failed: %s\n",
                    strerror(errno));
            exit(1);
        }
    }

    delete_all_connections(instance->connections_head);
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
    uint32_t local_ip;
    uint16_t local_port;
    const struct sockaddr_in *sin;
    struct relay_list relays = { .next = 0, };
    struct instance instance = {
        .connections_head = NULL,
        .relays = &relays,
    };

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
    instance.login_port = sin->sin_port;
    instance.login_ip = sin->sin_addr.s_addr;

    freeaddrinfo(ai);

    /* set up */

    setup_signal_handlers();

    /* call main loop */
    run_server(&instance, local_ip, local_port);

    return 0;
}
