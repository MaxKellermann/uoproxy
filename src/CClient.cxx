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
#include "LinkedServer.hxx"
#include "SocketConnect.hxx"
#include "ProxySocks.hxx"
#include "Server.hxx"
#include "Handler.hxx"
#include "Log.hxx"
#include "Instance.hxx"
#include "Config.hxx"

#include <assert.h>

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
            LogFormat(2, "auto-reconnecting\n");
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
        LogFormat(2, "server disconnected, auto-reconnecting\n");
        connection_speak_console(this, "uoproxy was disconnected, auto-reconnecting...");
        ScheduleReconnect();
    } else {
        LogFormat(1, "server disconnected\n");
        Destroy();
    }
}

int
Connection::Connect(const struct sockaddr *server_address,
                    size_t server_address_length,
                    uint32_t seed)
{
    assert(client.client == nullptr);

    int fd;

    const struct addrinfo *socks4_address =
        instance.config.socks4_address;
    if (socks4_address != nullptr) {
        fd = socket_connect(socks4_address->ai_family, SOCK_STREAM, 0,
                            socks4_address->ai_addr, socks4_address->ai_addrlen);
        if (fd < 0)
            return errno;

        if (!socks_connect(fd, server_address))
            return errno;
    } else {
        fd = socket_connect(server_address->sa_family, SOCK_STREAM, 0,
                            server_address, server_address_length);
        if (fd < 0)
            return errno;
    }

    client.Connect(fd, client_version, seed, *this);
    return 0;
}
