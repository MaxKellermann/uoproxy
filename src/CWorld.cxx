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
#include "Server.hxx"

static void
connection_delete_items(Connection *c)
{
    auto &world = c->client.world;

    world.items.clear_and_dispose([c](Item *i){
        const struct uo_packet_delete p{
            .cmd = PCK_Delete,
            .serial = i->serial,
        };

        for (auto &ls : c->servers) {
            if (ls.IsInGame())
                uo_server_send(ls.server, &p, sizeof(p));
        }

        delete i;
    });
}

static void
connection_delete_mobiles(Connection *c)
{
    auto &world = c->client.world;

    world.mobiles.clear_and_dispose([c](Mobile *m){
        const struct uo_packet_delete p{
            .cmd = PCK_Delete,
            .serial = m->serial,
        };

        for (auto &ls : c->servers) {
            if (ls.IsInGame())
                uo_server_send(ls.server, &p, sizeof(p));
        }

        delete m;
    });
}

void
connection_world_clear(Connection *c)
{
    connection_delete_items(c);
    connection_delete_mobiles(c);
}
