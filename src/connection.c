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
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>

#include "connection.h"
#include "server.h"
#include "client.h"
#include "handler.h"

int connection_new(struct instance *instance,
                   int server_socket,
                   struct connection **connectionp) {
    struct connection *c;
    int ret;

    c = calloc(1, sizeof(*c));
    if (c == NULL)
        return -ENOMEM;

    c->instance = instance;

    ret = uo_server_create(server_socket, &c->server);
    if (ret != 0) {
        connection_delete(c);
        return ret;
    }

    c->background = 1; /* XXX: testing this option */
    c->autoreconnect = 1; /* XXX: testing this option */

    *connectionp = c;

    return 0;
}

void connection_delete(struct connection *c) {
    if (c->server != NULL) {
        uo_server_dispose(c->server);
        c->server = NULL;
    }

    if (c->client != NULL) {
        uo_client_dispose(c->client);
        c->client = NULL;
    }

    connection_delete_items(c);
    connection_delete_mobiles(c);

    if (c->server_address != NULL) {
        if (c->server_address->ai_addr != NULL)
            free(c->server_address->ai_addr);
        free(c->server_address);
    }

    free(c);
}

void connection_invalidate(struct connection *c) {
    c->invalid = 1;
}

void connection_pre_select(struct connection *c, struct selectx *sx) {
    if (c->invalid)
        return;

    if (c->client != NULL &&
        !uo_client_alive(c->client)) {
        if (c->autoreconnect && c->in_game &&
            c->server_address != NULL) {
            uo_client_dispose(c->client);
            c->client = NULL;

            c->reconnecting = 1;

            if (c->server != NULL)
                uo_server_speak_console(c->server,
                                        "uoproxy was disconnected, auto-reconnecting...");

            connection_delete_items(c);
            connection_delete_mobiles(c);
        } else {
            printf("server disconnected\n");
            connection_invalidate(c);
        }
    }

    if (c->server != NULL &&
        !uo_server_alive(c->server)) {
        if (c->background && c->in_game) {
            fprintf(stderr, "client disconnected, backgrounding\n");
            uo_server_dispose(c->server);
            c->server = NULL;
        } else {
            fprintf(stderr, "client disconnected\n");
            connection_invalidate(c);
        }
    }

    if (c->client != NULL)
        uo_client_pre_select(c->client, sx);

    if (c->server != NULL)
        uo_server_pre_select(c->server, sx);
}

int connection_post_select(struct connection *c, struct selectx *sx) {
    void *p;
    size_t length;
    packet_action_t action;

    if (c->invalid)
        return 0;

    if (c->client != NULL) {
        uo_client_post_select(c->client, sx);
        while (c->client != NULL &&
               (p = uo_client_peek(c->client, &length)) != NULL) {
            uo_client_shift(c->client, length);

            action = handle_packet(server_packet_bindings,
                                   c, p, length);
            switch (action) {
            case PA_ACCEPT:
                if (c->server != NULL && !c->attaching)
                    uo_server_send(c->server, p, length);
                break;

            case PA_DROP:
                break;

            case PA_DISCONNECT:
                connection_invalidate(c);
                break;
            }
        }
    }

    if (c->server != NULL) {
        uo_server_post_select(c->server, sx);
        while (c->server != NULL &&
               (p = uo_server_peek(c->server, &length)) != NULL) {
            uo_server_shift(c->server, length);

            action = handle_packet(client_packet_bindings,
                                   c, p, length);
            switch (action) {
            case PA_ACCEPT:
                if (c->client != NULL && !c->reconnecting)
                    uo_client_send(c->client, p, length);
                break;

            case PA_DROP:
                break;

            case PA_DISCONNECT:
                connection_invalidate(c);
                break;
            }
        }
    }

    return 0;
}

void connection_idle(struct connection *c) {
    if (c->invalid)
        return;

    if (c->client == NULL) {
        if (c->reconnecting) {
            const u_int32_t seed = rand();
            int ret;

            assert(c->server_address != NULL);

            ret = uo_client_create(c->server_address, seed, &c->client);
            if (ret == 0) {
                struct uo_packet_account_login p = {
                    .cmd = PCK_AccountLogin,
                };

                printf("connected, doing AccountLogin\n");

                memcpy(p.username, c->username, sizeof(p.username));
                memcpy(p.password, c->password, sizeof(p.password));

                uo_client_send(c->client, &p, sizeof(p));
            } else {
                fprintf(stderr, "reconnect failed: %s\n", strerror(-ret));
            }
        }
    } else {
        struct uo_packet_ping ping;

        ping.cmd = PCK_Ping;
        ping.id = ++c->ping_request;

        printf("sending ping\n");
        uo_client_send(c->client, &ping, sizeof(ping));
    }
}
