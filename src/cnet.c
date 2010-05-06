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
#include "server.h"

#include <assert.h>

/** broadcast a message to all clients */
void connection_speak_console(struct connection *c, const char *msg) {
    struct linked_server *ls;

    list_for_each_entry(ls, &c->servers, siblings) {
        if (!ls->attaching && !ls->is_zombie) {
            assert(ls->server != NULL);

            uo_server_speak_console(ls->server, msg);
        }
    }
}

void
connection_broadcast_servers(struct connection *c,
                             const void *data, size_t length)
{
    struct linked_server *ls;

    list_for_each_entry(ls, &c->servers, siblings)
        if (!ls->attaching && !ls->is_zombie)
            uo_server_send(ls->server, data, length);
}

void connection_broadcast_servers_except(struct connection *c,
                                         const void *data, size_t length,
                                         struct uo_server *except) {
    struct linked_server *ls;

    assert(except != NULL);

    list_for_each_entry(ls, &c->servers, siblings)
        if (!ls->attaching && !ls->is_zombie && ls->server != except)
            uo_server_send(ls->server, data, length);
}

void
connection_broadcast_divert(struct connection *c,
                            enum protocol_version new_protocol,
                            const void *old_data, size_t old_length,
                            const void *new_data, size_t new_length)
{
    struct linked_server *ls;

    assert(new_protocol > PROTOCOL_UNKNOWN);
    assert(old_data != NULL);
    assert(old_length > 0);
    assert(new_data != NULL);
    assert(new_length > 0);

    list_for_each_entry(ls, &c->servers, siblings) {
        if (!ls->attaching && !ls->is_zombie) {
            if (ls->client_version.protocol >= new_protocol)
                uo_server_send(ls->server, new_data, new_length);
            else
                uo_server_send(ls->server, old_data, old_length);
        }
    }
}
