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
#include "CVersion.hxx"
#include "Log.hxx"

#include <assert.h>

static void
ping_event_callback(int, short, void *ctx) noexcept
{
    auto client = (StatefulClient *)ctx;
    struct uo_packet_ping ping;

    assert(client->client != nullptr);

    ping.cmd = PCK_Ping;
    ping.id = ++client->ping_request;

    LogFormat(2, "sending ping\n");
    uo_client_send(client->client, &ping, sizeof(ping));

    /* schedule next ping */
    client->SchedulePing();
}

StatefulClient::StatefulClient() noexcept
{
    evtimer_set(&ping_event, ping_event_callback, this);
}

void
StatefulClient::Connect(int fd, const ClientVersion &version,
                        uint32_t seed,
                        UO::ClientHandler &handler)
{
    assert(client == nullptr);

    const struct uo_packet_seed *seed_packet = version.seed;
    struct uo_packet_seed seed_buffer;

    if (seed_packet == nullptr &&
        version.IsDefined() &&
        version.protocol >= PROTOCOL_6_0_14) {
        seed_buffer.cmd = PCK_Seed;
        seed_buffer.seed = seed;

        if (version.protocol >= PROTOCOL_7) {
            seed_buffer.client_major = 7;
            seed_buffer.client_minor = 0;
            seed_buffer.client_revision = 10;
            seed_buffer.client_patch = 3;
        } else {
            seed_buffer.client_major = 6;
            seed_buffer.client_minor = 0;
            seed_buffer.client_revision = 14;
            seed_buffer.client_patch = 2;
        }

        seed_packet = &seed_buffer;
    }

    client = uo_client_create(fd, seed,
                              seed_packet,
                              handler);

    uo_client_set_protocol(client, version.protocol);

    SchedulePing();

}

void
StatefulClient::Disconnect() noexcept
{
    assert(client != nullptr);

    version_requested = false;

    event_del(&ping_event);

    uo_client_dispose(client);
    client = nullptr;
}
