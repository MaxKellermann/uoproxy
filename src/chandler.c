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

#include <sys/types.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "packets.h"
#include "handler.h"
#include "connection.h"
#include "client.h"
#include "server.h"
#include "relay.h"

static packet_action_t handle_account_login(struct connection *c,
                                            void *data, size_t length) {
    const struct uo_packet_login *p = data;
    int ret;

    assert(length == sizeof(*p));

    printf("account_login: username=%s password=%s\n",
           p->username, p->password);

    if (c->client != NULL) {
        fprintf(stderr, "already logged in\n");
        return PA_DISCONNECT;
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

        uo_server_send(c->server, &response,
                       sizeof(response));
        return PA_DROP;
    }

    return PA_ACCEPT;
}

static packet_action_t handle_game_login(struct connection *c,
                                         void *data, size_t length) {
    const struct uo_packet_game_login *p = data;
    int ret;
    const struct relay *relay;

    assert(length == sizeof(*p));

    printf("game_login: username=%s password=%s\n",
           p->username, p->password);

    if (c->client != NULL) {
        fprintf(stderr, "already logged in\n");
        return PA_DISCONNECT;
    }

    relay = relay_find(&relays, p->auth_id);
    if (relay == NULL) {
        fprintf(stderr, "invalid or expired auth_id: 0x%08x\n",
                p->auth_id);
        return PA_DISCONNECT;
    }

    c->server_ip = relay->server_ip;
    c->server_port = relay->server_port;

    ret = uo_client_create(c->server_ip, c->server_port,
                           uo_server_seed(c->server),
                           &c->client);
    if (ret != 0) {
        fprintf(stderr, "uo_client_create() failed: %s\n",
                strerror(-ret));
        return PA_DISCONNECT;
    }

    return PA_ACCEPT;
}

struct packet_binding client_packet_bindings[] = {
    { .cmd = PCK_AccountLogin,
      .handler = handle_account_login,
    },
    { .cmd = PCK_AccountLogin2,
      .handler = handle_account_login,
    },
    { .cmd = PCK_GameLogin,
      .handler = handle_game_login,
    },
    { .handler = NULL }
};

