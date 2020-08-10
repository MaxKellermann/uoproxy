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

#include "Server.hxx"
#include "SocketBuffer.hxx"
#include "Compression.hxx"
#include "packets.h"
#include "Log.hxx"
#include "compiler.h"
#include "SocketUtil.hxx"
#include "Encryption.hxx"

#include <utility>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#include <event.h>

namespace UO {

class Server {
public:
    SocketBuffer *sock = nullptr;
    uint32_t seed = 0;
    bool compression_enabled = false;

    struct encryption *encryption = encryption_new();

    enum protocol_version protocol_version = PROTOCOL_UNKNOWN;

    ServerHandler *handler;

    bool aborted = false;
    struct event abort_event;

    explicit Server(ServerHandler &_handler) noexcept
        :handler(&_handler)
    {
    }

    ~Server() noexcept {
        encryption_free(encryption);

        if (sock != nullptr)
            sock_buff_dispose(sock);

        if (aborted)
            evtimer_del(&abort_event);
    }
};

} // namespace UO

static void
uo_server_invoke_free(UO::Server *server)
{
    assert(server->handler != nullptr);

    auto *handler = std::exchange(server->handler, nullptr);
    handler->OnServerDisconnect();
}

static void
uo_server_abort_event_callback(int fd __attr_unused,
                               short event __attr_unused,
                               void *ctx)
{
    auto server = (UO::Server *)ctx;

    assert(server->aborted);

    uo_server_invoke_free(server);
}

static void
uo_server_abort(UO::Server *server)
{
    if (server->aborted)
        return;

    struct timeval tv = {
        .tv_sec = 0,
        .tv_usec = 0,
    };

    /* this is a trick to delay the destruction of this object until
       everything is done */
    evtimer_set(&server->abort_event,
                uo_server_abort_event_callback, server);
    evtimer_add(&server->abort_event, &tv);

    server->aborted = true;
}

static int
uo_server_is_aborted(UO::Server *server)
{
    return server->aborted;
}

static ssize_t
server_packets_from_buffer(UO::Server *server,
                           const unsigned char *data, size_t length)
{
    size_t consumed = 0;

    while (length > 0) {
        size_t packet_length = get_packet_length(server->protocol_version,
                                                 data, length);
        if (packet_length == PACKET_LENGTH_INVALID) {
            LogFormat(1, "malformed packet from client\n");
            log_hexdump(5, data, length);
            uo_server_abort(server);
            return 0;
        }

        LogFormat(9, "from client: 0x%02x length=%u\n",
            data[0], (unsigned)packet_length);

        if (packet_length == 0 || packet_length > length)
            break;

        log_hexdump(10, data, packet_length);

        if (!server->handler->OnServerPacket(data, packet_length))
            return -1;

        consumed += packet_length;
        data += packet_length;
        length -= packet_length;
    }

    return (ssize_t)consumed;
}

static size_t
server_sock_buff_data(const void *data0, size_t length, void *ctx)
{
    auto server = (UO::Server *)ctx;

    data0 = encryption_from_client(server->encryption, data0, length);
    if (data0 == nullptr)
        /* need more data */
        return 0;

    const unsigned char *data = (const unsigned char *)data0;
    size_t consumed = 0;

    if (server->seed == 0 && data[0] == 0xef) {
        /* client 6.0.5.0 sends a "0xef" seed packet instead of the
           raw 32 bit seed */
        auto p = (const struct uo_packet_seed *)data0;

        if (length < sizeof(*p))
            return 0;

        server->seed = p->seed;
        if (server->seed == 0) {
            LogFormat(2, "zero seed from client\n");
            uo_server_abort(server);
            return 0;
        }
    }

    if (server->seed == 0) {
        /* the first packet from a client is the seed, 4 bytes without
           header */
        if (length < 4)
            return 0;

        server->seed = *(const uint32_t*)(data + consumed);
        if (server->seed == 0) {
            LogFormat(2, "zero seed from client\n");
            uo_server_abort(server);
            return 0;
        }

        consumed += sizeof(uint32_t);
    }

    ssize_t nbytes = server_packets_from_buffer(server, data + consumed,
                                                length - consumed);
    if (nbytes < 0)
        return 0;

    return consumed + (size_t)nbytes;
}

static void
server_sock_buff_free(int error, void *ctx)
{
    auto server = (UO::Server *)ctx;

    if (error == 0)
        LogFormat(2, "client closed the connection\n");
    else
        log_error("error during communication with client", error);

    sock_buff_dispose(server->sock);
    server->sock = nullptr;

    uo_server_invoke_free(server);
}

static constexpr SocketBufferHandler server_sock_buff_handler = {
    .data = server_sock_buff_data,
    .free = server_sock_buff_free,
};

int
uo_server_create(int sockfd,
                 UO::ServerHandler &handler,
                 UO::Server **serverp)
{
    if (socket_set_nonblock(sockfd, 1) < 0 ||
        socket_set_nodelay(sockfd, 1) < 0)
        return errno;

    auto *server = new UO::Server(handler);
    if (server == nullptr)
        return ENOMEM;

    server->sock = sock_buff_create(sockfd, 8192, 65536,
                                    &server_sock_buff_handler, server);

    *serverp = server;

    return 0;
}

void uo_server_dispose(UO::Server *server) {
    delete server;
}

uint32_t uo_server_seed(const UO::Server *server) {
    return server->seed;
}

void uo_server_set_compression(UO::Server *server, bool comp) {
    server->compression_enabled = comp;
}

void
uo_server_set_protocol(UO::Server *server,
                       enum protocol_version protocol_version)
{
    assert(server->protocol_version == PROTOCOL_UNKNOWN);

    server->protocol_version = protocol_version;
}

uint32_t uo_server_getsockname(const UO::Server *server)
{
    return sock_buff_sockname(server->sock);
}

uint16_t uo_server_getsockport(const UO::Server *server)
{
    return sock_buff_port(server->sock);
}

void uo_server_send(UO::Server *server,
                    const void *src, size_t length) {
    assert(server->sock != nullptr || uo_server_is_aborted(server));
    assert(length > 0);
    assert(get_packet_length(server->protocol_version, src, length) == length);

    if (uo_server_is_aborted(server))
        return;

    LogFormat(9, "sending packet to client, length=%u\n", (unsigned)length);
    log_hexdump(10, src, length);

    if (server->compression_enabled) {
        size_t max_length;
        void *dest = sock_buff_write(server->sock, &max_length);
        if (dest == nullptr) {
            LogFormat(1, "output buffer full in uo_server_send()\n");
            uo_server_abort(server);
            return;
        }

        ssize_t nbytes = uo_compress((unsigned char *)dest, max_length,
                                     (const unsigned char *)src, length);
        if (nbytes < 0) {
            LogFormat(1, "uo_compress() failed\n");
            uo_server_abort(server);
            return;
        }

        sock_buff_append(server->sock, (size_t)nbytes);
    } else {
        if (!sock_buff_send(server->sock, src, length)) {
            LogFormat(1, "output buffer full in uo_server_send()\n");
            uo_server_abort(server);
        }
    }
}
