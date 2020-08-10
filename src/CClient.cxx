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
#include "SocketConnect.hxx"
#include "ProxySocks.hxx"
#include "Client.hxx"
#include "Server.hxx"
#include "Handler.hxx"
#include "Log.hxx"
#include "compiler.h"
#include "Instance.hxx"
#include "Config.hxx"

#include <assert.h>
#include <unistd.h>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#endif

static int
client_packet(const void *data, size_t length, void *ctx)
{
    auto c = (Connection *)ctx;
    packet_action_t action;

    assert(c->client.client != nullptr);

    action = handle_packet_from_server(server_packet_bindings,
                                       c, data, length);
    switch (action) {
    case PA_ACCEPT:
        if (!c->client.reconnecting) {
            LinkedServer *ls;
            list_for_each_entry(ls, &c->servers, siblings)
                if (!ls->attaching && !ls->is_zombie)
                    uo_server_send(ls->server, data, length);
        }

        break;

    case PA_DROP:
        break;

    case PA_DISCONNECT:
        LogFormat(2, "aborting connection to server after packet 0x%x\n",
                  *(const unsigned char*)data);
        log_hexdump(6, data, length);

        if (c->autoreconnect && c->in_game) {
            LogFormat(2, "auto-reconnecting\n");
            connection_disconnect(c);
            connection_reconnect_delayed(c);
        } else {
            connection_delete(c);
        }
        return -1;

    case PA_DELETED:
        return -1;
    }

    return 0;
}

static void
client_free(void *ctx)
{
    auto c = (Connection *)ctx;

    assert(c->client.client != nullptr);

    if (c->autoreconnect && c->in_game) {
        LogFormat(2, "server disconnected, auto-reconnecting\n");
        connection_speak_console(c, "uoproxy was disconnected, auto-reconnecting...");
        connection_disconnect(c);
        connection_reconnect_delayed(c);
    } else {
        LogFormat(1, "server disconnected\n");
        connection_delete(c);
    }
}

static constexpr UO::ClientHandler client_handler = {
    .packet = client_packet,
    .free = client_free,
};

static void
connection_ping_event_callback(int fd __attr_unused,
                               short event __attr_unused, void *ctx)
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

int
connection_client_connect(Connection *c,
                          const struct sockaddr *server_address,
                          size_t server_address_length,
                          uint32_t seed)
{
    int ret;
    struct timeval tv;

    assert(c->client.client == nullptr);

    int fd;

    const struct addrinfo *socks4_address =
        c->instance->config->socks4_address;
    if (socks4_address != nullptr) {
        fd = socket_connect(socks4_address->ai_family, SOCK_STREAM, 0,
                            socks4_address->ai_addr, socks4_address->ai_addrlen);
        if (fd < 0)
            return -fd;

        if (!socks_connect(fd, server_address))
            return -1;
    } else {
        fd = socket_connect(server_address->sa_family, SOCK_STREAM, 0,
                            server_address, server_address_length);
        if (fd < 0)
            return -fd;
    }

    const struct uo_packet_seed *seed_packet = c->client_version.seed;
    struct uo_packet_seed seed_buffer;

    if (seed_packet == nullptr &&
        client_version_defined(&c->client_version) &&
        c->client_version.protocol >= PROTOCOL_6_0_14) {
        seed_buffer.cmd = PCK_Seed;
        seed_buffer.seed = seed;

        if (c->client_version.protocol >= PROTOCOL_7) {
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

    ret = uo_client_create(fd, seed,
                           seed_packet,
                           &client_handler, c,
                           &c->client.client);
    if (ret != 0) {
        close(fd);
        return ret;
    }

    uo_client_set_protocol(c->client.client,
                           c->client_version.protocol);

    tv.tv_sec = 30;
    tv.tv_usec = 0;
    evtimer_set(&c->client.ping_event, connection_ping_event_callback,
                &c->client);
    evtimer_add(&c->client.ping_event, &tv);

    return 0;
}

void
connection_client_disconnect(StatefulClient *client)
{
    assert(client->client != nullptr);

    if (client->reconnecting) {
        event_del(&client->reconnect_event);
        client->reconnecting = false;
    }

    client->version_requested = false;

    event_del(&client->ping_event);

    uo_client_dispose(client->client);
    client->client = nullptr;
}
