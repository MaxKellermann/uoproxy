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

#include "Client.hxx"
#include "SocketBuffer.hxx"
#include "Compression.hxx"
#include "PacketLengths.h"
#include "PacketType.hxx"
#include "pversion.h"
#include "FifoBuffer.hxx"
#include "Log.hxx"
#include "compiler.h"
#include "SocketUtil.hxx"

#include <utility>

#include <assert.h>
#include <stdlib.h>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#endif

#include <event.h>

static void
uo_client_abort_event_callback(int fd,
                               short event,
                               void *ctx);

namespace UO {

class Client {
public:
    SocketBuffer *sock = nullptr;
    bool compression_enabled = false;
    struct uo_decompression decompression;
    struct fifo_buffer *decompressed_buffer = nullptr;

    enum protocol_version protocol_version = PROTOCOL_UNKNOWN;

    ClientHandler *handler;

    bool aborted = false;
    struct event abort_event;

    explicit Client(ClientHandler &_handler) noexcept
        :handler(&_handler)
    {
        uo_decompression_init(&decompression);

        evtimer_set(&abort_event,
                    uo_client_abort_event_callback, this);
    }

    ~Client() noexcept {
        if (decompressed_buffer != nullptr)
            fifo_buffer_free(decompressed_buffer);

        if (sock != nullptr)
            sock_buff_dispose(sock);

        evtimer_del(&abort_event);
    }
};

} // namespace UO

static void
uo_client_invoke_free(UO::Client *client)
{
    assert(client->handler != nullptr);

    auto *handler = std::exchange(client->handler, nullptr);
    handler->OnClientDisconnect();
}

static void
uo_client_abort_event_callback(int fd __attr_unused,
                               short event __attr_unused,
                               void *ctx)
{
    auto client = (UO::Client *)ctx;

    assert(client->aborted);

    uo_client_invoke_free(client);
}

static void
uo_client_abort(UO::Client *client)
{
    if (client->aborted)
        return;

    struct timeval tv = {
        .tv_sec = 0,
        .tv_usec = 0,
    };

    /* this is a trick to delay the destruction of this object until
       everything is done */
    evtimer_add(&client->abort_event, &tv);

    client->aborted = true;
}

static int
uo_client_is_aborted(UO::Client *client)
{
    return client->aborted;
}

static ssize_t
client_decompress(UO::Client *client,
                  const unsigned char *data, size_t length)
{
    size_t max_length;
    ssize_t nbytes;

    auto dest = (unsigned char *)fifo_buffer_write(client->decompressed_buffer, &max_length);
    if (dest == nullptr) {
        LogFormat(1, "decompression buffer full\n");
        uo_client_abort(client);
        return -1;
    }

    nbytes = uo_decompress(&client->decompression,
                           dest, max_length,
                           data, length);
    if (nbytes < 0) {
        LogFormat(1, "decompression failed\n");
        uo_client_abort(client);
        return -1;
    }

    fifo_buffer_append(client->decompressed_buffer, (size_t)nbytes);

    return (size_t)length;
}

static ssize_t
client_packets_from_buffer(UO::Client *client,
                           const unsigned char *data, size_t length)
{
    size_t consumed = 0, packet_length;

    while (length > 0) {
        packet_length = get_packet_length(client->protocol_version,
                                          data, length);
        if (packet_length == PACKET_LENGTH_INVALID) {
            LogFormat(1, "malformed packet from server\n");
            log_hexdump(5, data, length);
            uo_client_abort(client);
            return 0;
        }

        LogFormat(9, "from server: 0x%02x length=%u\n",
            data[0], (unsigned)packet_length);

        if (packet_length == 0 || packet_length > length)
            break;

        log_hexdump(10, data, packet_length);

        if (!client->handler->OnClientPacket(data, packet_length))
            return -1;

        consumed += packet_length;
        data += packet_length;
        length -= packet_length;
    }

    return (ssize_t)consumed;
}

static size_t
client_sock_buff_data(const void *data0, size_t length, void *ctx)
{
    auto client = (UO::Client *)ctx;
    const unsigned char *data = (const unsigned char *)data0;

    if (client->compression_enabled) {
        ssize_t nbytes;
        size_t consumed;

        nbytes = client_decompress(client, data, length);
        if (nbytes <= 0)
            return 0;
        consumed = (size_t)nbytes;

        data = (const unsigned char *)fifo_buffer_read(client->decompressed_buffer, &length);
        if (data == nullptr)
            return consumed;

        nbytes = client_packets_from_buffer(client, data, length);
        if (nbytes < 0)
            return 0;

        fifo_buffer_consume(client->decompressed_buffer, (size_t)nbytes);

        return consumed;
    } else {
        ssize_t nbytes = client_packets_from_buffer(client, data, length);
        if (nbytes < 0)
            return 0;

        return nbytes;
    }
}

static void
client_sock_buff_free(int error, void *ctx)
{
    auto client = (UO::Client *)ctx;

    if (error == 0)
        LogFormat(2, "server closed the connection\n");
    else
        log_error("error during communication with server", error);

    sock_buff_dispose(client->sock);
    client->sock = nullptr;

    uo_client_invoke_free(client);
}

static constexpr SocketBufferHandler client_sock_buff_handler = {
    .data = client_sock_buff_data,
    .free = client_sock_buff_free,
};

UO::Client *
uo_client_create(int fd, uint32_t seed,
                 const struct uo_packet_seed *seed6,
                 UO::ClientHandler &handler)
{
    socket_set_nodelay(fd, 1);

    auto *client = new UO::Client(handler);

    client->sock = sock_buff_create(fd, 8192, 65536,
                                    &client_sock_buff_handler, client);

    client->decompressed_buffer = fifo_buffer_new(65536);

    /* seed must be the first 4 bytes, and it must be flushed */
    if (seed6 != nullptr) {
        struct uo_packet_seed p = *seed6;
        p.seed = seed;
        uo_client_send(client, &p, sizeof(p));
    } else
        uo_client_send(client, (unsigned char*)&seed, sizeof(seed));

    return client;
}

void uo_client_dispose(UO::Client *client) {
    delete client;
}

void
uo_client_set_protocol(UO::Client *client,
                       enum protocol_version protocol_version)
{
    assert(client->protocol_version == PROTOCOL_UNKNOWN);

    client->protocol_version = protocol_version;
}

void uo_client_send(UO::Client *client,
                    const void *src, size_t length) {
    assert(client->sock != nullptr || uo_client_is_aborted(client));
    assert(length > 0);

    if (uo_client_is_aborted(client))
        return;

    LogFormat(9, "sending packet to server, length=%u\n", (unsigned)length);
    log_hexdump(10, src, length);

    if (*(const unsigned char*)src == PCK_GameLogin)
        client->compression_enabled = true;

    if (!sock_buff_send(client->sock, src, length)) {
        LogFormat(1, "output buffer full in uo_client_send()\n");
        uo_client_abort(client);
    }
}
