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

#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>

#include "connection.h"
#include "server.h"
#include "client.h"
#include "relay.h"

int connection_new(int server_socket,
                   u_int32_t local_ip, u_int16_t local_port,
                   u_int32_t server_ip, u_int16_t server_port,
                   struct connection **connectionp) {
    struct connection *c;
    int ret;

    c = calloc(1, sizeof(*c));
    if (c == NULL)
        return -ENOMEM;

    c->local_ip = local_ip;
    c->local_port = local_port;
    ret = uo_server_create(server_socket, &c->server);
    if (ret != 0) {
        connection_delete(c);
        return ret;
    }

    c->server_ip = server_ip;
    c->server_port = server_port;

    *connectionp = c;

    return 0;
}

void connection_delete(struct connection *c) {
    if (c->server != NULL)
        uo_server_dispose(c->server);
    if (c->client != NULL)
        uo_client_dispose(c->client);
    free(c);
}

void connection_invalidate(struct connection *c) {
    c->invalid = 1;

    if (c->server != NULL) {
        uo_server_dispose(c->server);
        c->server = NULL;
    }

    if (c->client != NULL) {
        uo_client_dispose(c->client);
        c->client = NULL;
    }
}
