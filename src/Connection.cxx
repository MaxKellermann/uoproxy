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
#include "Instance.hxx"
#include "Config.hxx"
#include "Log.hxx"

#include <assert.h>
#include <stdlib.h>
#include <errno.h>

int connection_new(Instance *instance,
                   int server_socket,
                   Connection **connectionp) {
    int ret;

    auto *c = new Connection(*instance,
                             instance->config.background,
                             instance->config.autoreconnect);

    if (instance->config.client_version != nullptr) {
        ret = client_version_set(&c->client_version,
                                 instance->config.client_version);
        if (ret > 0)
            LogFormat(2, "configured client version '%s', protocol '%s'\n",
                      c->client_version.packet->version,
                      protocol_name(c->client_version.protocol));
    }

    auto *ls = new LinkedServer(server_socket);
    c->Add(*ls);

    *connectionp = c;

    return 0;
}

Connection::~Connection() noexcept
{
    servers.clear_and_dispose([](LinkedServer *ls){ delete ls; });

    Disconnect();

    if (client.reconnecting)
        event_del(&client.reconnect_event);
}

void
connection_delete(Connection *c)
{
    c->unlink();
    delete c;
}
