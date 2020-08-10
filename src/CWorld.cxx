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

static void
connection_delete_items(Connection *c)
{
    auto &world = c->client.world;
    struct uo_packet_delete p = { .cmd = PCK_Delete };
    Item *i, *n;

    list_for_each_entry_safe(i, n, &world.items, siblings) {
        p.serial = i->serial;

        LinkedServer *ls;
        list_for_each_entry(ls, &c->servers, siblings) {
            if (!ls->attaching && !ls->is_zombie)
                uo_server_send(ls->server, &p, sizeof(p));
        }

        world.RemoveItem(*i);
    }
}

static void
connection_delete_mobiles(Connection *c)
{
    auto &world = c->client.world;
    struct uo_packet_delete p = { .cmd = PCK_Delete };
    Mobile *m, *n;

    list_for_each_entry_safe(m, n, &world.mobiles, siblings) {
        p.serial = m->serial;

        LinkedServer *ls;
        list_for_each_entry(ls, &c->servers, siblings) {
            if (!ls->attaching && !ls->is_zombie)
                uo_server_send(ls->server, &p, sizeof(p));
        }

        world.RemoveMobile(*m);
    }
}

void
connection_world_clear(Connection *c)
{
    connection_delete_items(c);
    connection_delete_mobiles(c);
}
