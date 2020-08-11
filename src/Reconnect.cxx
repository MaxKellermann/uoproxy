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

    client.reconnecting = false;
    event_del(&reconnect_event);

    client.Disconnect();
    ClearWorld();
}

void
Connection::DoReconnect() noexcept
{
    const auto &config = instance.config;
    uint32_t seed;
    int ret;

    assert(IsInGame());
    assert(client.reconnecting);
    assert(client.client == nullptr);

    if (client_version.seed != nullptr)
        seed = client_version.seed->seed;
    else
        seed = 0xc0a80102; /* 192.168.1.2 */

    if (config.login_address == nullptr) {
        /* connect to game server */
        struct addrinfo *server_address
            = config.game_servers[server_index].address;

        assert(config.game_servers != nullptr);
        assert(server_index < config.num_game_servers);

        ret = Connect(server_address->ai_addr,
                      server_address->ai_addrlen, seed);
        if (ret == 0) {
            const struct uo_packet_game_login p = {
                .cmd = PCK_GameLogin,
                .auth_id = seed,
                .credentials = credentials,
            };

            LogFormat(2, "connected, doing GameLogin\n");

            uo_client_send(client.client, &p, sizeof(p));
        } else {
            log_error("reconnect failed", ret);
            client.reconnecting = false;
            ScheduleReconnect();
        }
    } else {
        /* connect to login server */
        ret = Connect(config.login_address->ai_addr,
                      config.login_address->ai_addrlen,
                      seed);
        if (ret == 0) {
            const struct uo_packet_account_login p = {
                .cmd = PCK_AccountLogin,
                .credentials = credentials,
                .unknown1 = {},
            };

            LogFormat(2, "connected, doing AccountLogin\n");

            uo_client_send(client.client, &p, sizeof(p));
        } else {
            log_error("reconnect failed", ret);
            client.reconnecting = false;
            ScheduleReconnect();
        }
    }
}

void
Connection::ReconnectTimerCallback(int, short, void *ctx) noexcept
{
    auto c = (Connection *)ctx;

    c->DoReconnect();
}

void
Connection::Reconnect()
{
    if (client.reconnecting)
        return;

    Disconnect();

    assert(IsInGame());
    assert(client.client == nullptr);

    client.reconnecting = true;

    DoReconnect();
}

void
Connection::ScheduleReconnect() noexcept
{
    if (client.reconnecting)
        return;

    Disconnect();

    assert(IsInGame());
    assert(client.client == nullptr);

    client.reconnecting = true;

    static constexpr struct timeval tv{5, 0};
    evtimer_add(&reconnect_event, &tv);
}
