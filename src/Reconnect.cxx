/*
 * uoproxy
 *
 * Copyright 2005-2020 Max Kellermann <max.kellermann@gmail.com>
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

#include "Connection.hxx"
#include "Instance.hxx"
#include "Client.hxx"
#include "Config.hxx"
#include "Log.hxx"

#include <assert.h>
#include <time.h>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#endif

void
Connection::Disconnect() noexcept
{
    if (client.client == nullptr)
        return;

    client.Disconnect();
    ClearWorld();
}

static void
connection_try_reconnect(Connection *c)
{
    const auto &config = c->instance.config;
    uint32_t seed;
    int ret;

    assert(c->in_game);
    assert(c->client.reconnecting);
    assert(c->client.client == nullptr);

    if (c->client_version.seed != nullptr)
        seed = c->client_version.seed->seed;
    else
        seed = 0xc0a80102; /* 192.168.1.2 */

    if (config.login_address == nullptr) {
        /* connect to game server */
        struct addrinfo *server_address
            = config.game_servers[c->server_index].address;

        assert(config.game_servers != nullptr);
        assert(c->server_index < config.num_game_servers);

        ret = c->Connect(server_address->ai_addr,
                         server_address->ai_addrlen, seed);
        if (ret == 0) {
            const struct uo_packet_game_login p = {
                .cmd = PCK_GameLogin,
                .auth_id = seed,
                .credentials = c->credentials,
            };

            LogFormat(2, "connected, doing GameLogin\n");

            uo_client_send(c->client.client, &p, sizeof(p));
        } else {
            log_error("reconnect failed", ret);
            c->client.reconnecting = false;
            c->ScheduleReconnect();
        }
    } else {
        /* connect to login server */
        ret = c->Connect(config.login_address->ai_addr,
                         config.login_address->ai_addrlen,
                         seed);
        if (ret == 0) {
            const struct uo_packet_account_login p = {
                .cmd = PCK_AccountLogin,
                .credentials = c->credentials,
                .unknown1 = {},
            };

            LogFormat(2, "connected, doing AccountLogin\n");

            uo_client_send(c->client.client, &p, sizeof(p));
        } else {
            log_error("reconnect failed", ret);
            c->client.reconnecting = false;
            c->ScheduleReconnect();
        }
    }
}

static void
connection_reconnect_event_callback(int, short, void *ctx) noexcept
{
    auto c = (Connection *)ctx;

    connection_try_reconnect(c);
}

void
Connection::Reconnect()
{
    if (client.reconnecting)
        return;

    Disconnect();

    assert(in_game);
    assert(client.client == nullptr);

    client.reconnecting = true;

    connection_try_reconnect(this);
}

void
Connection::ScheduleReconnect() noexcept
{
    if (client.reconnecting)
        return;

    Disconnect();

    assert(in_game);
    assert(client.client == nullptr);

    client.reconnecting = true;

    static constexpr struct timeval tv{5, 0};
    evtimer_set(&client.reconnect_event,
                connection_reconnect_event_callback, this);
    evtimer_add(&client.reconnect_event, &tv);
}
