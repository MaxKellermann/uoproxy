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

#include "connection.h"
#include "instance.h"
#include "server.h"
#include "config.h"
#include "poison.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

static void
connection_free(struct connection *c);

int connection_new(struct instance *instance,
                   int server_socket,
                   struct connection **connectionp) {
    struct connection *c;
    struct linked_server *ls;
    int ret;

    c = calloc(1, sizeof(*c));
    if (c == NULL)
        return ENOMEM;

    c->instance = instance;
    c->background = instance->config->background;
    c->autoreconnect = instance->config->autoreconnect;

    INIT_LIST_HEAD(&c->client.world.items);
    INIT_LIST_HEAD(&c->client.world.mobiles);
    INIT_LIST_HEAD(&c->servers);

    ls = connection_server_new(c, server_socket);
    if (ls == NULL) {
        ret = errno;
        connection_free(c);
        return ret;
    }

    connection_check(c);

    *connectionp = c;

    return 0;
}

static void
connection_free(struct connection *c)
{
    struct linked_server *ls, *n;

    connection_check(c);

    list_for_each_entry_safe(ls, n, &c->servers, siblings)
        connection_server_dispose(c, ls);

    connection_disconnect(c);

    if (c->client.reconnecting)
        event_del(&c->client.reconnect_event);

    client_version_free(&c->client_version);

    poison(c, sizeof(*c));
    free(c);
}

#ifndef NDEBUG
void connection_check(const struct connection *c) {
    assert(c != NULL);
    assert(c->instance != NULL);

    if (!c->in_game) {
        /* when not yet in-game, there can only be one connection */
        assert(list_empty(&c->servers) ||
               c->servers.next->next == &c->servers);
    }
}
#endif

void
connection_delete(struct connection *c)
{
    list_del(&c->siblings);
    connection_free(c);
}
