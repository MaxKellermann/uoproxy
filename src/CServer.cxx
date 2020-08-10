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

void
connection_server_dispose(Connection *c, LinkedServer *ls)
{
    connection_server_remove(c, ls);

    delete ls;
}
