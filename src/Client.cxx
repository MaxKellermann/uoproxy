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
#include "packets.h"
#include "pversion.h"
#include "FifoBuffer.hxx"
#include "Log.hxx"
#include "compiler.h"
#include "SocketUtil.hxx"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#endif

#include <event.h>

struct uo_client {
    struct sock_buff *sock;
    bool compression_enabled;
    struct uo_decompression decompression;
    struct fifo_buffer *decompressed_buffer;

    enum protocol_version protocol_version;

    const struct uo_client_handler *handler;
    void *handler_ctx;

    bool aborted;
    struct event abort_event;
};

static void
uo_client_invoke_free(struct uo_client *client)
{
    const struct uo_client_handler *handler;

    assert(client->handler != nullptr);

    handler = client->handler;
    client->handler = nullptr;

    handler->free(client->handler_ctx);
}

static void
uo_client_abort_event_callback(int fd __attr_unused,
                               short event __attr_unused,
                               void *ctx)
{
    auto client = (struct uo_client *)ctx;

    assert(client->aborted);

    uo_client_invoke_free(client);
}

static void
uo_client_abort(struct uo_client *client)
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
uo_client_is_aborted(struct uo_client *client)
{
    return client->aborted;
}

static ssize_t
client_decompress(struct uo_client *client,
                  const unsigned char *data, size_t length)
{
    size_t max_length;
    ssize_t nbytes;

    auto dest = (unsigned char *)fifo_buffer_write(client->decompressed_buffer, &max_length);
    if (dest == nullptr) {
        log(1, "decompression buffer full\n");
        uo_client_abort(client);
        return -1;
    }

    nbytes = uo_decompress(&client->decompression,
                           dest, max_length,
                           data, length);
    if (nbytes < 0) {
        log(1, "decompression failed\n");
        uo_client_abort(client);
        return -1;
    }

    fifo_buffer_append(client->decompressed_buffer, (size_t)nbytes);

    return (size_t)length;
}

static ssize_t
client_packets_from_buffer(struct uo_client *client,
                           const unsigned char *data, size_t length)
{
    size_t consumed = 0, packet_length;
    int ret;

    while (length > 0) {
        packet_length = get_packet_length(client->protocol_version,
                                          data, length);
        if (packet_length == PACKET_LENGTH_INVALID) {
            log(1, "malformed packet from server\n");
            log_hexdump(5, data, length);
            uo_client_abort(client);
            return 0;
        }

        log(9, "from server: 0x%02x length=%u\n",
            data[0], (unsigned)packet_length);

        if (packet_length == 0 || packet_length > length)
            break;

        log_hexdump(10, data, packet_length);

        ret = client->handler->packet(data, packet_length,
                                      client->handler_ctx);
        if (ret < 0)
            return ret;

        consumed += packet_length;
        data += packet_length;
        length -= packet_length;
    }

    return (ssize_t)consumed;
}

static size_t
client_sock_buff_data(const void *data0, size_t length, void *ctx)
{
    auto client = (struct uo_client *)ctx;
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
    auto client = (struct uo_client *)ctx;

    if (error == 0)
        log(2, "server closed the connection\n");
    else
        log_error("error during communication with server", error);

    sock_buff_dispose(client->sock);
    client->sock = nullptr;

    uo_client_invoke_free(client);
}

struct sock_buff_handler client_sock_buff_handler = {
    .data = client_sock_buff_data,
    .free = client_sock_buff_free,
};

int
uo_client_create(int fd, uint32_t seed,
                 const struct uo_packet_seed *seed6,
                 const struct uo_client_handler *handler,
                 void *handler_ctx,
                 struct uo_client **clientp)
{
    int ret;
    struct uo_client *client;

    assert(handler != nullptr);
    assert(handler->packet != nullptr);
    assert(handler->free != nullptr);

    ret = socket_set_nonblock(fd, 1);
    if (ret < 0)
        return errno;

    ret = socket_set_nodelay(fd, 1);
    if (ret < 0)
        return errno;

    client = (struct uo_client*)calloc(1, sizeof(*client));
    if (client == nullptr)
        return ENOMEM;

    client->sock = sock_buff_create(fd, 8192, 65536,
                                    &client_sock_buff_handler, client);
    if (client->sock == nullptr) {
        free(client);
        return errno;
    }

    uo_decompression_init(&client->decompression);
    client->decompressed_buffer = fifo_buffer_new(65536);
    if (client->decompressed_buffer == nullptr) {
        uo_client_dispose(client);
        return ENOMEM;
    }

    client->handler = handler;
    client->handler_ctx = handler_ctx;

    evtimer_set(&client->abort_event,
                uo_client_abort_event_callback, client);

    *clientp = client;

    /* seed must be the first 4 bytes, and it must be flushed */
    if (seed6 != nullptr) {
        struct uo_packet_seed p = *seed6;
        p.seed = seed;
        uo_client_send(client, &p, sizeof(p));
    } else
        uo_client_send(client, (unsigned char*)&seed, sizeof(seed));

    return 0;
}

void uo_client_dispose(struct uo_client *client) {
    fifo_buffer_free(client->decompressed_buffer);

    if (client->sock != nullptr)
        sock_buff_dispose(client->sock);

    evtimer_del(&client->abort_event);

    free(client);
}

void
uo_client_set_protocol(struct uo_client *client,
                       enum protocol_version protocol_version)
{
    assert(client->protocol_version == PROTOCOL_UNKNOWN);

    client->protocol_version = protocol_version;
}

void uo_client_send(struct uo_client *client,
                    const void *src, size_t length) {
    assert(client->sock != nullptr || uo_client_is_aborted(client));
    assert(length > 0);

    if (uo_client_is_aborted(client))
        return;

    log(9, "sending packet to server, length=%u\n", (unsigned)length);
    log_hexdump(10, src, length);

    if (*(const unsigned char*)src == PCK_GameLogin)
        client->compression_enabled = true;

    if (!sock_buff_send(client->sock, src, length)) {
        log(1, "output buffer full in uo_client_send()\n");
        uo_client_abort(client);
    }
}
