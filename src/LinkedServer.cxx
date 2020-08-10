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

#include "LinkedServer.hxx"
#include "Connection.hxx"
#include "Handler.hxx"
#include "Log.hxx"

#include <cassert>

LinkedServer::~LinkedServer() noexcept
{
    if (is_zombie)
        evtimer_del(&zombie_timeout);

    if (server != nullptr)
        uo_server_dispose(server);
}

bool
LinkedServer::OnServerPacket(const void *data, size_t length)
{
    Connection *c = connection;

    assert(c != nullptr);
    assert(!is_zombie);

    const auto action = handle_packet_from_client(client_packet_bindings,
                                                  this, data, length);
    switch (action) {
    case PacketAction::ACCEPT:
        if (c->client.client != nullptr &&
            (!c->client.reconnecting ||
             *(const unsigned char*)data == PCK_ClientVersion))
            uo_client_send(c->client.client, data, length);
        break;

    case PacketAction::DROP:
        break;

    case PacketAction::DISCONNECT:
        LogFormat(2, "aborting connection to client after packet 0x%x\n",
                  *(const unsigned char*)data);
        log_hexdump(6, data, length);

        connection_server_dispose(c, this);
        if (c->servers.empty()) {
            if (c->background)
                LogFormat(1, "backgrounding\n");
            else
                connection_delete(c);
        }
        return false;

    case PacketAction::DELETED:
        return false;
    }

    return true;
}

void
LinkedServer::OnServerDisconnect() noexcept
{
    Connection *c = connection;

    assert(c != nullptr);

    connection_walk_server_removed(&c->walk, this);

    if (expecting_reconnect) {
        LogFormat(2, "client disconnected, zombifying server connection for 5 seconds\n");
        connection_server_zombify(c, this);
    } else if (c->servers.iterator_to(*this) != c->servers.begin() ||
               std::next(c->servers.iterator_to(*this)) != c->servers.end()) {
        LogFormat(2, "client disconnected, server connection still in use\n");
        connection_server_dispose(c, this);
    } else if (c->background && c->in_game) {
        LogFormat(1, "client disconnected, backgrounding\n");
        connection_server_dispose(c, this);
    } else {
        LogFormat(1, "last client disconnected, removing connection\n");
        connection_server_dispose(c, this);
        connection_delete(c);
    }
}
