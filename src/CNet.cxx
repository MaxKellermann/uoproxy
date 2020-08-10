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
#include "Server.hxx"

#include <assert.h>

/** broadcast a message to all clients */
void
connection_speak_console(Connection *c, const char *msg)
{
    LinkedServer *ls;

    list_for_each_entry(ls, &c->servers, siblings) {
        if (!ls->attaching && !ls->is_zombie) {
            assert(ls->server != nullptr);

            uo_server_speak_console(ls->server, msg);
        }
    }
}

void
connection_broadcast_servers(Connection *c,
                             const void *data, size_t length)
{
    LinkedServer *ls;

    list_for_each_entry(ls, &c->servers, siblings)
        if (!ls->attaching && !ls->is_zombie)
            uo_server_send(ls->server, data, length);
}

void
connection_broadcast_servers_except(Connection *c,
                                    const void *data, size_t length,
                                    struct uo_server *except)
{
    LinkedServer *ls;

    assert(except != nullptr);

    list_for_each_entry(ls, &c->servers, siblings)
        if (!ls->attaching && !ls->is_zombie && ls->server != except)
            uo_server_send(ls->server, data, length);
}

void
connection_broadcast_divert(Connection *c,
                            enum protocol_version new_protocol,
                            const void *old_data, size_t old_length,
                            const void *new_data, size_t new_length)
{
    assert(new_protocol > PROTOCOL_UNKNOWN);
    assert(old_data != nullptr);
    assert(old_length > 0);
    assert(new_data != nullptr);
    assert(new_length > 0);

    LinkedServer *ls;
    list_for_each_entry(ls, &c->servers, siblings) {
        if (!ls->attaching && !ls->is_zombie) {
            if (ls->client_version.protocol >= new_protocol)
                uo_server_send(ls->server, new_data, new_length);
            else
                uo_server_send(ls->server, old_data, old_length);
        }
    }
}
