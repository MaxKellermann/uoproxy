/*
 * uoproxy
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

#include "log.h"

#include <assert.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "instance.h"
#include "connection.h"
#include "server.h"
#include "client.h"
#include "handler.h"
#include "config.h"

int connection_new(struct instance *instance,
                   int server_socket,
                   struct connection **connectionp) {
    struct connection *c;
    struct uo_server *server = NULL;
    struct linked_server *ls;
    int ret;

    c = calloc(1, sizeof(*c));
    if (c == NULL)
        return -ENOMEM;

    c->instance = instance;
    c->background = instance->config->background;
    c->autoreconnect = instance->config->autoreconnect;

    INIT_LIST_HEAD(&c->items);
    INIT_LIST_HEAD(&c->mobiles);
    INIT_LIST_HEAD(&c->servers);

    ls = connection_server_new(c, server_socket);
    if (ls == NULL) {
        ret = -errno;
        uo_server_dispose(server);
        connection_delete(c);
        return ret;
    }

    connection_check(c);

    *connectionp = c;

    return 0;
}

void connection_delete(struct connection *c) {
    struct linked_server *ls, *n;

    connection_check(c);

    list_for_each_entry_safe(ls, n, &c->servers, siblings) {
        list_del(&ls->siblings);

        if (ls->server != NULL)
            uo_server_dispose(ls->server);
        free(ls);
    }

    if (c->client != NULL) {
        uo_client_dispose(c->client);
        c->client = NULL;
    }

    connection_delete_items(c);
    connection_delete_mobiles(c);

    free(c);
}

#ifndef NDEBUG
void connection_check(const struct connection *c) {
    assert(c != NULL);
    assert(c->instance != NULL);

    if (!c->in_game) {
        /* when not yet in-game, there can only be one connection */
        assert(list_empty(&c->servers) ||
               c->servers.next->next == &c->servers);
    }
}
#endif

void connection_invalidate(struct connection *c) {
    list_del(&c->siblings);
    connection_delete(c);
}

void connection_pre_select(struct connection *c, struct selectx *sx) {
    struct linked_server *ls, *n;

    connection_check(c);

    if (c->client != NULL)
        uo_client_pre_select(c->client, sx);

    list_for_each_entry_safe(ls, n, &c->servers, siblings) {
        assert(ls->server != NULL);

        uo_server_pre_select(ls->server, sx);
    }
}

int connection_post_select(struct connection *c, struct selectx *sx) {
    struct linked_server *ls, *n;

    connection_check(c);

    if (c->client != NULL)
        uo_client_post_select(c->client, sx);

    assert(c->current_server == NULL);

    list_for_each_entry_safe(ls, n, &c->servers, siblings)
        uo_server_post_select(ls->server, sx);

    return 0;
}

void connection_idle(struct connection *c, time_t now) {
    connection_check(c);

    if (c->client == NULL) {
        if (c->reconnecting && now >= c->next_reconnect) {
            struct config *config = c->instance->config;
            const u_int32_t seed = rand();
            int ret;

            assert(c->in_game);

            if (config->login_address == NULL) {
                /* connect to game server */
                struct addrinfo *server_address
                    = config->game_servers[c->server_index].address;

                assert(config->game_servers != NULL);
                assert(c->server_index < config->num_game_servers);

                ret = connection_client_connect(c, server_address, seed);
                if (ret == 0) {
                    struct uo_packet_game_login p = {
                        .cmd = PCK_GameLogin,
                    };

                    if (verbose >= 2)
                        printf("connected, doing GameLogin\n");

                    memcpy(p.username, c->username, sizeof(p.username));
                    memcpy(p.password, c->password, sizeof(p.password));

                    uo_client_send(c->client, &p, sizeof(p));
                } else {
                    if (verbose >= 1)
                        fprintf(stderr, "reconnect failed: %s\n",
                                strerror(-ret));
                    connection_reconnect(c);
                }
            } else {
                /* connect to login server */
                ret = connection_client_connect(c,
                                                c->instance->config->login_address,
                                                seed);
                if (ret == 0) {
                    struct uo_packet_account_login p = {
                        .cmd = PCK_AccountLogin,
                    };

                    if (verbose >= 2)
                        printf("connected, doing AccountLogin\n");

                    memcpy(p.username, c->username, sizeof(p.username));
                    memcpy(p.password, c->password, sizeof(p.password));

                    uo_client_send(c->client, &p, sizeof(p));
                } else {
                    if (verbose >= 1)
                        fprintf(stderr, "reconnect failed: %s\n",
                                strerror(-ret));
                    connection_reconnect(c);
                }
            }
        }
    } else if (now >= c->next_ping) {
        struct uo_packet_ping ping;

        ping.cmd = PCK_Ping;
        ping.id = ++c->ping_request;

        if (verbose >= 2)
            printf("sending ping\n");
        uo_client_send(c->client, &ping, sizeof(ping));

        c->next_ping = now + 30;
        instance_schedule(c->instance, 29);
    }
}
