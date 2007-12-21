/*
 * uoproxy
 *
 * (c) 2005 Max Kellermann <max@duempel.org>
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

#include <assert.h>

#include "connection.h"
#include "server.h"

/** broadcast a message to all clients */
void connection_speak_console(struct connection *c, const char *msg) {
    struct linked_server *ls;

    for (ls = c->servers_head; ls != NULL; ls = ls->next) {
        if (!ls->invalid && !ls->attaching) {
            assert(ls->server != NULL);

            uo_server_speak_console(ls->server, msg);
        }
    }
}

void connection_broadcast_servers_except(struct connection *c,
                                         const void *data, size_t length,
                                         struct uo_server *except) {
    struct linked_server *ls;

    assert(except != NULL);

    for (ls = c->servers_head; ls != NULL; ls = ls->next)
        if (!ls->invalid && !ls->attaching && ls->server != except)
            uo_server_send(ls->server, data, length);
}
