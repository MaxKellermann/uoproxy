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
#include <errno.h>

struct linked_server *
connection_add_server(struct connection *c, struct uo_server *server)
{
    struct linked_server *ls;

    assert(c != NULL);
    assert(server != NULL);

    connection_check(c);

    ls = calloc(1, sizeof(*ls));
    if (ls == NULL)
        return NULL;

    ls->server = server;

    list_add(&ls->siblings, &c->servers);

    return ls;
}

struct linked_server *
connection_server_new(struct connection *c, int fd)
{
    struct uo_server *server;
    int ret;

    ret = uo_server_create(fd, &server);
    if (ret != 0) {
        errno = ret;
        return NULL;
    }

    return connection_add_server(c, server);
}
