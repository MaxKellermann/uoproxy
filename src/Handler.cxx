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

#include "Handler.hxx"

PacketAction
handle_packet_from_server(const struct client_packet_binding *bindings,
                          Connection &c,
                          const void *data, size_t length)
{
    const unsigned char cmd
        = *(const unsigned char*)data;

    for (; bindings->handler != nullptr; bindings++) {
        if (bindings->cmd == cmd)
            return bindings->handler(c, data, length);
    }

    return PacketAction::ACCEPT;
}

PacketAction
handle_packet_from_client(const struct server_packet_binding *bindings,
                          LinkedServer &ls,
                          const void *data, size_t length)
{
    const unsigned char cmd
        = *(const unsigned char*)data;

    for (; bindings->handler != nullptr; bindings++) {
        if (bindings->cmd == cmd)
            return bindings->handler(ls, data, length);
    }

    return PacketAction::ACCEPT;
}
