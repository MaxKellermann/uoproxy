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
#include "client.h"
#include "handler.h"
#include "log.h"

#include <assert.h>
#include <stdlib.h>
#include <errno.h>

static int
server_packet(void *data, size_t length, void *ctx)
{
    struct linked_server *ls = ctx;
    struct connection *c = ls->connection;
    packet_action_t action;

    assert(c != NULL);

    c->current_server = ls;
    action = handle_packet(client_packet_bindings,
                           c, data, length);
    c->current_server = NULL;

    switch (action) {
    case PA_ACCEPT:
        if (c->client != NULL && !c->reconnecting)
            uo_client_send(c->client, data, length);
        break;

    case PA_DROP:
        break;

    case PA_DISCONNECT:
        /* XXX: only disconnect this server */
        fprintf(stderr, "aborting connection to client\n");
        connection_invalidate(c);
        return -1;
    }

    return 0;
}

static const struct uo_server_handler server_handler = {
    .packet = server_packet,
};

void
connection_server_add(struct connection *c, struct linked_server *ls)
{
    assert(ls->connection == NULL);

    list_add(&ls->siblings, &c->servers);
    ls->connection = c;
}

void
connection_server_remove(struct connection *c, struct linked_server *ls)
{
    connection_check(c);
    assert(ls != NULL);
    assert(c == ls->connection);

    if (c->current_server == ls)
        c->current_server = NULL;

    connection_walk_server_removed(&c->walk, ls);

    ls->connection = NULL;
    list_del(&ls->siblings);
}

struct linked_server *
connection_server_new(struct connection *c, int fd)
{
    struct linked_server *ls;
    int ret;

    ls = calloc(1, sizeof(*ls));
    if (ls == NULL)
        return NULL;

    ret = uo_server_create(fd,
                           &server_handler, ls,
                           &ls->server);
    if (ret != 0) {
        free(ls);
        errno = ret;
        return NULL;
    }

    connection_server_add(c, ls);
    return ls;
}

void
connection_server_dispose(struct connection *c, struct linked_server *ls)
{
    assert(ls != NULL);

    (void)c;

    list_del(&ls->siblings);

    if (ls->server != NULL)
        uo_server_dispose(ls->server);

    free(ls);
}
