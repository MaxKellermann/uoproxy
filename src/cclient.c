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
#include "client.h"
#include "server.h"
#include "handler.h"
#include "log.h"

#include <assert.h>

static int
client_packet(void *data, size_t length, void *ctx)
{
    struct connection *c = ctx;
    packet_action_t action;
    struct linked_server *ls;

    action = handle_packet(server_packet_bindings,
                           c, data, length);
    switch (action) {
    case PA_ACCEPT:
        if (!c->reconnecting)
            list_for_each_entry(ls, &c->servers, siblings)
                if (!ls->attaching)
                    uo_server_send(ls->server, data, length);
        break;

    case PA_DROP:
        break;

    case PA_DISCONNECT:
        log(2, "aborting connection to server\n");
        connection_invalidate(c);
        return -1;
    }

    return 0;
}

static void
client_free(void *ctx)
{
    struct connection *c = ctx;

    if (c->autoreconnect && c->in_game) {
        log(2, "server disconnected, auto-reconnecting\n");
        connection_speak_console(c, "uoproxy was disconnected, auto-reconnecting...");
        connection_reconnect(c);
    } else {
        log(1, "server disconnected\n");
        connection_invalidate(c);
    }
}

static const struct uo_client_handler client_handler = {
    .packet = client_packet,
    .free = client_free,
};

int
connection_client_connect(struct connection *c,
                          const struct addrinfo *server_address,
                          u_int32_t seed)
{
    assert(c->client == NULL);

    return uo_client_create(server_address, seed,
                            &client_handler, c,
                            &c->client);
}
