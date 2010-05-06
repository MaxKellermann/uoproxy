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

static void
connection_delete_items(struct connection *c)
{
    struct uo_packet_delete p = { .cmd = PCK_Delete };
    struct item *i, *n;
    struct linked_server *ls;

    list_for_each_entry_safe(i, n, &c->client.world.items, siblings) {
        p.serial = i->serial;

        list_for_each_entry(ls, &c->servers, siblings) {
            if (!ls->attaching && !ls->is_zombie)
                uo_server_send(ls->server, &p, sizeof(p));
        }

        world_remove_item(i);
    }
}

static void
connection_delete_mobiles(struct connection *c)
{
    struct uo_packet_delete p = { .cmd = PCK_Delete };
    struct mobile *m, *n;
    struct linked_server *ls;

    list_for_each_entry_safe(m, n, &c->client.world.mobiles, siblings) {
        p.serial = m->serial;

        list_for_each_entry(ls, &c->servers, siblings) {
            if (!ls->attaching && !ls->is_zombie)
                uo_server_send(ls->server, &p, sizeof(p));
        }

        world_remove_mobile(m);
    }
}

void
connection_world_clear(struct connection *c)
{
    connection_delete_items(c);
    connection_delete_mobiles(c);
}
