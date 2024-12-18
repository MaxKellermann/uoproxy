// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Connection.hxx"
#include "Instance.hxx"
#include "Client.hxx"
#include "Config.hxx"
#include "Log.hxx"

#include <assert.h>
#include <time.h>

#ifdef _WIN32
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

    if (client.version.seed != nullptr)
        seed = client.version.seed->seed;
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
    Disconnect();

    assert(IsInGame());
    assert(client.client == nullptr);

    client.reconnecting = true;

    DoReconnect();
}

void
Connection::ScheduleReconnect() noexcept
{
    Disconnect();

    assert(IsInGame());
    assert(client.client == nullptr);

    client.reconnecting = true;

    static constexpr struct timeval tv{5, 0};
    evtimer_add(&reconnect_event, &tv);
}
