// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Connection.hxx"
#include "LinkedServer.hxx"
#include "SocketConnect.hxx"
#include "ProxySocks.hxx"
#include "Server.hxx"
#include "Handler.hxx"
#include "Log.hxx"
#include "Instance.hxx"
#include "Config.hxx"
#include "net/SocketAddress.hxx"

#include <assert.h>
#include <unistd.h>

bool
Connection::OnClientPacket(const void *data, size_t length)
{
    assert(client.client != nullptr);

    const auto action = handle_packet_from_server(server_packet_bindings,
                                                  *this, data, length);
    switch (action) {
    case PacketAction::ACCEPT:
        if (!client.reconnecting)
            BroadcastToInGameClients(data, length);

        break;

    case PacketAction::DROP:
        break;

    case PacketAction::DISCONNECT:
        LogFormat(2, "aborting connection to server after packet 0x%x\n",
                  *(const unsigned char*)data);
        log_hexdump(6, data, length);

        if (autoreconnect && IsInGame()) {
            Log(2, "auto-reconnecting\n");
            ScheduleReconnect();
        } else {
            Destroy();
        }
        return false;

    case PacketAction::DELETED:
        return false;
    }

    return true;
}

void
Connection::OnClientDisconnect() noexcept
{
    assert(client.client != nullptr);

    if (autoreconnect && IsInGame()) {
        Log(2, "server disconnected, auto-reconnecting\n");
        connection_speak_console(this, "uoproxy was disconnected, auto-reconnecting...");
        ScheduleReconnect();
    } else {
        Log(1, "server disconnected\n");
        Destroy();
    }
}

void
Connection::Connect(SocketAddress server_address,
                    uint32_t seed, bool for_game_login)
{
    assert(client.client == nullptr);

    UniqueSocketDescriptor fd;

    const struct addrinfo *socks4_address =
        instance.config.socks4_address;
    if (socks4_address != nullptr) {
        fd = socket_connect(socks4_address->ai_family, SOCK_STREAM, 0,
                            {socks4_address->ai_addr, socks4_address->ai_addrlen});

        socks_connect(fd, server_address);
    } else {
        fd = socket_connect(server_address.GetFamily(), SOCK_STREAM, 0,
                            server_address);
    }

    client.Connect(std::move(fd), seed, for_game_login, *this);
}
