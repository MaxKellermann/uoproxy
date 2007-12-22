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

#include <assert.h>
#include <time.h>

#include "connection.h"
#include "instance.h"
#include "client.h"
#include "config.h"
#include "log.h"

#include <stdlib.h>

void connection_disconnect(struct connection *c) {
    if (c->client == NULL)
        return;

    connection_delete_items(c);
    connection_delete_mobiles(c);

    uo_client_dispose(c->client);
    c->client = NULL;
}

void
connection_try_reconnect(struct connection *c)
{
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

            log(2, "connected, doing GameLogin\n");

            memcpy(p.username, c->username, sizeof(p.username));
            memcpy(p.password, c->password, sizeof(p.password));

            uo_client_send(c->client, &p, sizeof(p));
        } else {
            log_error("reconnect failed", ret);
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

            log(2, "connected, doing AccountLogin\n");

            memcpy(p.username, c->username, sizeof(p.username));
            memcpy(p.password, c->password, sizeof(p.password));

            uo_client_send(c->client, &p, sizeof(p));
        } else {
            log_error("reconnect failed", ret);
            connection_reconnect(c);
        }
    }
}

void connection_reconnect(struct connection *c) {
    connection_disconnect(c);

    assert(c->in_game);
    assert(c->client == NULL);

    c->reconnecting = 1;
    c->next_reconnect = time(NULL) + 4;
    instance_schedule(c->instance, 5);
}
