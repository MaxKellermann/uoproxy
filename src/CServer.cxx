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
#include "Handler.hxx"
#include "Log.hxx"

#include <assert.h>
#include <stdlib.h>

void
connection_server_add(Connection *c, LinkedServer *ls)
{
    assert(ls->connection == nullptr);

    c->servers.push_front(*ls);
    ls->connection = c;
}

void
connection_server_remove(Connection *c, LinkedServer *ls)
{
    connection_check(c);
    assert(ls != nullptr);
    assert(c == ls->connection);

    connection_walk_server_removed(&c->walk, ls);

    ls->connection = nullptr;
    ls->unlink();
}

LinkedServer *
connection_server_new(Connection *c, int fd)
{
    auto *ls = new LinkedServer(fd);
    connection_server_add(c, ls);
    return ls;
}

static void
zombie_timeout_event_callback(int fd __attr_unused,
                              short event __attr_unused,
                              void *ctx)
{
    LinkedServer *ls = (LinkedServer *)ctx;
    ls->expecting_reconnect = false;
    ls->OnServerDisconnect();
}

void
connection_server_zombify(Connection *c, LinkedServer *ls)
{
    struct timeval tv;
    connection_check(c);
    assert(ls != nullptr);
    assert(c == ls->connection);

    ls->is_zombie = true;
    evtimer_set(&ls->zombie_timeout, zombie_timeout_event_callback, ls);
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    evtimer_add(&ls->zombie_timeout, &tv);
}

void
connection_server_dispose(Connection *c, LinkedServer *ls)
{
    connection_server_remove(c, ls);

    delete ls;
}
