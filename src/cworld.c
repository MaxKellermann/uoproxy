/*
 * uoproxy
 *
 * (c) 2005-2007 Max Kellermann <max@duempel.org>
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void connection_delete_items(struct connection *c) {
    struct uo_packet_delete p = { .cmd = PCK_Delete };
    struct item *i, *n;
    struct linked_server *ls;

    list_for_each_entry_safe(i, n, &c->client.world.items, siblings) {
        p.serial = i->serial;

        list_for_each_entry(ls, &c->servers, siblings) {
            if (!ls->attaching)
                uo_server_send(ls->server, &p, sizeof(p));
        }

        list_del(&i->siblings);
        free(i);
    }
}

static void free_mobile(struct mobile *m) {
    assert(m != NULL);
    assert(m->serial != 0);

    if (m->packet_mobile_incoming != NULL)
        free(m->packet_mobile_incoming);
    if (m->packet_mobile_status != NULL)
        free(m->packet_mobile_status);

#ifndef NDEBUG
    memset(m, 0, sizeof(*m));
#endif

    free(m);
}

void connection_delete_mobiles(struct connection *c) {
    struct uo_packet_delete p = { .cmd = PCK_Delete };
    struct mobile *m, *n;
    struct linked_server *ls;

    list_for_each_entry_safe(m, n, &c->client.world.mobiles, siblings) {
        p.serial = m->serial;

        list_for_each_entry(ls, &c->servers, siblings) {
            if (!ls->attaching)
                uo_server_send(ls->server, &p, sizeof(p));
        }

        list_del(&m->siblings);
        free_mobile(m);
    }
}
