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
#include "Instance.hxx"
#include "Server.hxx"
#include "Config.hxx"
#include "Log.hxx"

#include <assert.h>
#include <stdlib.h>
#include <errno.h>

static void
connection_free(Connection *c);

int connection_new(Instance *instance,
                   int server_socket,
                   Connection **connectionp) {
    int ret;

    Connection *c = (Connection *)calloc(1, sizeof(*c));
    if (c == nullptr)
        return ENOMEM;

    c->instance = instance;
    c->background = instance->config->background;
    c->autoreconnect = instance->config->autoreconnect;

    INIT_LIST_HEAD(&c->client.world.items);
    INIT_LIST_HEAD(&c->client.world.mobiles);
    INIT_LIST_HEAD(&c->servers);

    if (instance->config->client_version != nullptr) {
        ret = client_version_set(&c->client_version,
                                 instance->config->client_version);
        if (ret > 0)
            LogFormat(2, "configured client version '%s', protocol '%s'\n",
                      c->client_version.packet->version,
                      protocol_name(c->client_version.protocol));
    }

    auto *ls = connection_server_new(c, server_socket);
    if (ls == nullptr) {
        ret = errno;
        connection_free(c);
        return ret;
    }

    connection_check(c);

    *connectionp = c;

    return 0;
}

static void
connection_free(Connection *c)
{
    LinkedServer *ls, *n;

    connection_check(c);

    list_for_each_entry_safe(ls, n, &c->servers, siblings)
        connection_server_dispose(c, ls);

    connection_disconnect(c);

    if (c->client.reconnecting)
        event_del(&c->client.reconnect_event);

    client_version_free(&c->client_version);

    free(c);
}

#ifndef NDEBUG
void connection_check(const Connection *c) {
    assert(c != nullptr);
    assert(c->instance != nullptr);

    if (!c->in_game) {
        /* when not yet in-game, there can only be one connection
           unless we are doing the razor workaround, in which case we keep
           zombies around that we later delete*/
        assert(list_empty(&c->servers) ||
               c->servers.next->next == &c->servers ||
               c->instance->config->razor_workaround);
    }
}
#endif

void
connection_delete(Connection *c)
{
    list_del(&c->siblings);
    connection_free(c);
}
