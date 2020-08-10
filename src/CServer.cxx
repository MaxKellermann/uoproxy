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
#include "Client.hxx"
#include "Handler.hxx"
#include "Log.hxx"

#include <assert.h>
#include <stdlib.h>
#include <errno.h>

static int
server_packet(const void *data, size_t length, void *ctx)
{
    auto ls = (LinkedServer *)ctx;
    Connection *c = ls->connection;
    packet_action_t action;

    assert(c != nullptr);
    assert(!ls->is_zombie);

    action = handle_packet_from_client(client_packet_bindings,
                                       ls, data, length);
    switch (action) {
    case PA_ACCEPT:
        if (c->client.client != nullptr &&
            (!c->client.reconnecting ||
             *(const unsigned char*)data == PCK_ClientVersion))
            uo_client_send(c->client.client, data, length);
        break;

    case PA_DROP:
        break;

    case PA_DISCONNECT:
        LogFormat(2, "aborting connection to client after packet 0x%x\n",
                  *(const unsigned char*)data);
        log_hexdump(6, data, length);

        connection_server_dispose(c, ls);
        if (list_empty(&c->servers)) {
            if (c->background)
                LogFormat(1, "backgrounding\n");
            else
                connection_delete(c);
        }
        return -1;

    case PA_DELETED:
        return -1;
    }

    return 0;
}

static void
server_free(void *ctx)
{
    auto ls = (LinkedServer *)ctx;
    Connection *c = ls->connection;

    assert(c != nullptr);

    connection_walk_server_removed(&c->walk, ls);

    if (ls->expecting_reconnect) {
        LogFormat(2, "client disconnected, zombifying server connection for 5 seconds\n");
        connection_server_zombify(c, ls);
    } else if (ls->siblings.next != &c->servers || ls->siblings.prev != &c->servers) {
        LogFormat(2, "client disconnected, server connection still in use\n");
        connection_server_dispose(c, ls);
    } else if (c->background && c->in_game) {
        LogFormat(1, "client disconnected, backgrounding\n");
        connection_server_dispose(c, ls);
    } else {
        LogFormat(1, "last client disconnected, removing connection\n");
        connection_server_dispose(c, ls);
        connection_delete(c);
    }
}

static constexpr UO::ServerHandler server_handler = {
    .packet = server_packet,
    .free = server_free,
};

void
connection_server_add(Connection *c, LinkedServer *ls)
{
    assert(ls->connection == nullptr);

    list_add(&ls->siblings, &c->servers);
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
    list_del(&ls->siblings);
}

LinkedServer *
connection_server_new(Connection *c, int fd)
{
    int ret;

    auto *ls = new LinkedServer();

    ret = uo_server_create(fd,
                           &server_handler, ls,
                           &ls->server);
    if (ret != 0) {
        delete ls;
        errno = ret;
        return nullptr;
    }

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
    server_free(ls);
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

LinkedServer::~LinkedServer() noexcept
{
    if (is_zombie)
        evtimer_del(&zombie_timeout);

    if (server != nullptr)
        uo_server_dispose(server);
}

void
connection_server_dispose(Connection *c, LinkedServer *ls)
{
    connection_server_remove(c, ls);

    delete ls;
}
