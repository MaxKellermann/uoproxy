/*
 * uoproxy
 * $Id$
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
#include <time.h>

#include "connection.h"
#include "instance.h"
#include "client.h"

void connection_disconnect(struct connection *c) {
    if (c->client == NULL)
        return;

    connection_delete_items(c);
    connection_delete_mobiles(c);

    uo_client_dispose(c->client);
    c->client = NULL;
}

void connection_reconnect(struct connection *c) {
    connection_disconnect(c);

    assert(c->client == NULL);

    if (c->server_address == NULL || !c->in_game)
        return;

    c->reconnecting = 1;
    c->next_reconnect = time(NULL) + 19;
    instance_schedule(c->instance, 20);
}
