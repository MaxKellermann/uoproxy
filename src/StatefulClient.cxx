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

#include "StatefulClient.hxx"
#include "Client.hxx"
#include "Log.hxx"

#include <assert.h>

static void
ping_event_callback(int, short, void *ctx) noexcept
{
    auto client = (StatefulClient *)ctx;
    struct uo_packet_ping ping;
    struct timeval tv;

    assert(client->client != nullptr);

    ping.cmd = PCK_Ping;
    ping.id = ++client->ping_request;

    LogFormat(2, "sending ping\n");
    uo_client_send(client->client, &ping, sizeof(ping));

    /* schedule next ping */
    tv.tv_sec = 30;
    tv.tv_usec = 0;
    event_add(&client->ping_event, &tv);
}

StatefulClient::StatefulClient() noexcept
{
    evtimer_set(&ping_event, ping_event_callback, this);
}

void
StatefulClient::Disconnect() noexcept
{
    assert(client != nullptr);

    if (reconnecting) {
        event_del(&reconnect_event);
        reconnecting = false;
    }

    version_requested = false;

    event_del(&ping_event);

    uo_client_dispose(client);
    client = nullptr;
}
