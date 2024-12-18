// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

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
    auto *ls = new LinkedServer(server_socket);

    auto *c = new Connection(*instance,
                             instance->config.background,
                             instance->config.autoreconnect);

    if (instance->config.client_version != nullptr) {
        c->client.version.Set(instance->config.client_version);
        ls->LogF(2, "configured client version '%s', protocol '%s'",
                 c->client.version.packet->version,
                 protocol_name(c->client.version.protocol));
    }

    c->Add(*ls);

    *connectionp = c;

    return 0;
}

Connection::~Connection() noexcept
{
    servers.clear_and_dispose([](LinkedServer *ls){ delete ls; });

    Disconnect();

    evtimer_del(&reconnect_event);
}
