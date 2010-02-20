/*
 * uoproxy
 *
 * (c) 2005-2010 Max Kellermann <max@duempel.org>
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

#include "connection.h"
#include "instance.h"
#include "client.h"
#include "config.h"
#include "log.h"
#include "compiler.h"

#include <assert.h>
#include <time.h>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#endif

void connection_disconnect(struct connection *c) {
    if (c->client.client == NULL)
        return;

    connection_client_disconnect(&c->client);
    connection_world_clear(c);
}

static void
connection_try_reconnect(struct connection *c)
{
    struct config *config = c->instance->config;
    uint32_t seed;
    int ret;

    assert(c->in_game);
    assert(c->client.reconnecting);
    assert(c->client.client == NULL);

    if (c->client_version.seed != NULL)
        seed = c->client_version.seed->seed;
    else
        seed = htonl(0xc0a80102); /* 192.168.1.2 */

    if (config->login_address == NULL) {
        /* connect to game server */
        struct addrinfo *server_address
            = config->game_servers[c->server_index].address;

        assert(config->game_servers != NULL);
        assert(c->server_index < config->num_game_servers);

        ret = connection_client_connect(c, server_address->ai_addr,
                                        server_address->ai_addrlen, seed);
        if (ret == 0) {
            struct uo_packet_game_login p = {
                .cmd = PCK_GameLogin,
                .auth_id = seed,
            };

            log(2, "connected, doing GameLogin\n");

            memcpy(p.username, c->username, sizeof(p.username));
            memcpy(p.password, c->password, sizeof(p.password));

            uo_client_send(c->client.client, &p, sizeof(p));
        } else {
            log_error("reconnect failed", ret);
            c->client.reconnecting = false;
            connection_reconnect_delayed(c);
        }
    } else {
        /* connect to login server */
        ret = connection_client_connect(c, config->login_address->ai_addr,
                                        config->login_address->ai_addrlen,
                                        seed);
        if (ret == 0) {
            struct uo_packet_account_login p = {
                .cmd = PCK_AccountLogin,
            };

            log(2, "connected, doing AccountLogin\n");

            memcpy(p.username, c->username, sizeof(p.username));
            memcpy(p.password, c->password, sizeof(p.password));

            uo_client_send(c->client.client, &p, sizeof(p));
        } else {
            log_error("reconnect failed", ret);
            c->client.reconnecting = false;
            connection_reconnect_delayed(c);
        }
    }
}

static void
connection_reconnect_event_callback(int fd __attr_unused,
                                    short event __attr_unused, void *ctx)
{
    struct connection *c = ctx;

    connection_try_reconnect(c);
}

void connection_reconnect(struct connection *c) {
    if (c->client.reconnecting)
        return;

    connection_disconnect(c);

    assert(c->in_game);
    assert(c->client.client == NULL);

    c->client.reconnecting = true;

    connection_try_reconnect(c);
}

void
connection_reconnect_delayed(struct connection *c)
{
    struct timeval tv;

    if (c->client.reconnecting)
        return;

    connection_disconnect(c);

    assert(c->in_game);
    assert(c->client.client == NULL);

    c->client.reconnecting = true;

    tv.tv_sec = 5;
    tv.tv_usec = 0;
    evtimer_set(&c->client.reconnect_event,
                connection_reconnect_event_callback, c);
    evtimer_add(&c->client.reconnect_event, &tv);
}
