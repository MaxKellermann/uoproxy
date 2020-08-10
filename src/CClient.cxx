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
#include "compiler.h"
#include "Instance.hxx"
#include "Config.hxx"

#include <assert.h>

bool
Connection::OnClientPacket(const void *data, size_t length)
{
    packet_action_t action;

    assert(client.client != nullptr);

    action = handle_packet_from_server(server_packet_bindings,
                                       this, data, length);
    switch (action) {
    case PA_ACCEPT:
        if (!client.reconnecting) {
            for (auto &ls : servers)
                if (ls.IsInGame())
                    uo_server_send(ls.server, data, length);
        }

        break;

    case PA_DROP:
        break;

    case PA_DISCONNECT:
        LogFormat(2, "aborting connection to server after packet 0x%x\n",
                  *(const unsigned char*)data);
        log_hexdump(6, data, length);

        if (autoreconnect && in_game) {
            LogFormat(2, "auto-reconnecting\n");
            connection_disconnect(this);
            connection_reconnect_delayed(this);
        } else {
            connection_delete(this);
        }
        return false;

    case PA_DELETED:
        return false;
    }

    return true;
}

void
Connection::OnClientDisconnect() noexcept
{
    assert(client.client != nullptr);

    if (autoreconnect && in_game) {
        LogFormat(2, "server disconnected, auto-reconnecting\n");
        connection_speak_console(this, "uoproxy was disconnected, auto-reconnecting...");
        connection_disconnect(this);
        connection_reconnect_delayed(this);
    } else {
        LogFormat(1, "server disconnected\n");
        connection_delete(this);
    }
}

int
connection_client_connect(Connection *c,
                          const struct sockaddr *server_address,
                          size_t server_address_length,
                          uint32_t seed)
{
    assert(c->client.client == nullptr);

    int fd;

    const struct addrinfo *socks4_address =
        c->instance->config->socks4_address;
    if (socks4_address != nullptr) {
        fd = socket_connect(socks4_address->ai_family, SOCK_STREAM, 0,
                            socks4_address->ai_addr, socks4_address->ai_addrlen);
        if (fd < 0)
            return -fd;

        if (!socks_connect(fd, server_address))
            return -1;
    } else {
        fd = socket_connect(server_address->sa_family, SOCK_STREAM, 0,
                            server_address, server_address_length);
        if (fd < 0)
            return -fd;
    }

    c->client.Connect(fd, c->client_version, seed, *c);
    return 0;
}
