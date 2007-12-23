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

#include "client.h"
#include "sockbuff.h"
#include "compression.h"
#include "packets.h"
#include "dump.h"
#include "fifo-buffer.h"
#include "log.h"
#include "compiler.h"
#include "socket-util.h"

#include <sys/socket.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <event.h>

struct uo_client {
    struct sock_buff *sock;
    int compression_enabled;
    struct uo_decompression decompression;
    fifo_buffer_t decompressed_buffer;

    const struct uo_client_handler *handler;
    void *handler_ctx;

    struct event abort_event;
};

static void
uo_client_invoke_free(struct uo_client *client)
{
    const struct uo_client_handler *handler;

    assert(client->handler != NULL);

    handler = client->handler;
    client->handler = NULL;

    handler->free(client->handler_ctx);
}

static void
uo_client_abort_event_callback(int fd __attr_unused,
                               short event __attr_unused,
                               void *ctx)
{
    struct uo_client *client = ctx;

    uo_client_invoke_free(client);
}

static void
uo_client_abort(struct uo_client *client)
{
    struct timeval tv = {
        .tv_sec = 0,
        .tv_usec = 0,
    };

    /* this is a trick to delay the destruction of this object until
       everything is done */
    evtimer_set(&client->abort_event,
                uo_client_abort_event_callback, client);
    evtimer_add(&client->abort_event, &tv);
}

static int
uo_client_is_aborted(struct uo_client *client)
{
    return client->abort_event.ev_events != 0;
}

static ssize_t
client_decompress(struct uo_client *client,
                  const unsigned char *data, size_t length)
{
    unsigned char *dest;
    size_t max_length;
    ssize_t nbytes;

    dest = fifo_buffer_write(client->decompressed_buffer, &max_length);
    if (dest == NULL) {
        log(1, "decompression buffer full\n");
        uo_client_abort(client);
        return -1;
    }

    nbytes = uo_decompress(&client->decompression,
                           dest, max_length,
                           data, length);
    if (nbytes < 0) {
        fprintf(stderr, "decompression failed\n");
        uo_client_abort(client);
        return -1;
    }

    sock_buff_event_setup(client->sock);
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
        packet_length = get_packet_length(data, length);
        if (packet_length == PACKET_LENGTH_INVALID) {
            fprintf(stderr, "malformed packet from client:\n");
            fhexdump(stderr, "  ", data, length);
            fflush(stderr);
            uo_client_abort(client);
            return 0;
        }

#ifdef DUMP_HEADERS
        printf("from server: 0x%02x length=%zu\n",
               data[0], packet_length);
#endif

        if (packet_length == 0 || packet_length > length)
            break;

#ifdef DUMP_SERVER_RECEIVE
        fhexdump(stdout, "  ", data, packet_length);
        fflush(stdout);
#endif

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

static ssize_t
client_sock_buff_data(const void *data0, size_t length, void *ctx)
{
    struct uo_client *client = ctx;
    const unsigned char *data = data0;

    if (client->compression_enabled) {
        ssize_t nbytes;
        size_t consumed;

        nbytes = client_decompress(client, data, length);
        if (nbytes <= 0)
            return 0;
        consumed = (size_t)nbytes;

        data = fifo_buffer_read(client->decompressed_buffer, &length);
        if (data == NULL)
            return consumed;

        nbytes = client_packets_from_buffer(client, data, length);
        if (nbytes < 0)
            return nbytes;

        fifo_buffer_consume(client->decompressed_buffer, (size_t)nbytes);

        return consumed;
    } else {
        return client_packets_from_buffer(client, data, length);
    }
}

static void
client_sock_buff_free(int error, void *ctx)
{
    struct uo_client *client = ctx;

    if (error == 0)
        log(2, "server closed the connection\n");
    else
        log_error("error during communication with server", error);

    sock_buff_dispose(client->sock);
    client->sock = NULL;

    uo_client_invoke_free(client);
}

struct sock_buff_handler client_sock_buff_handler = {
    .data = client_sock_buff_data,
    .free = client_sock_buff_free,
};

int uo_client_create(const struct addrinfo *server_address,
                     uint32_t seed,
                     const struct uo_client_handler *handler,
                     void *handler_ctx,
                     struct uo_client **clientp) {
    int sockfd, ret;
    struct uo_client *client;

    assert(handler != NULL);
    assert(handler->packet != NULL);
    assert(handler->free != NULL);

    sockfd = socket(server_address->ai_family, SOCK_STREAM, 0);
    if (sockfd < 0)
        return errno;

    ret = connect(sockfd, server_address->ai_addr,
                  server_address->ai_addrlen);
    if (ret < 0) {
        int save_errno = errno;
        close(sockfd);
        return save_errno;
    }

    ret = socket_set_nonblock(sockfd, 1);
    if (ret < 0) {
        int save_errno = errno;
        close(sockfd);
        return save_errno;
    }

    client = (struct uo_client*)calloc(1, sizeof(*client));
    if (client == NULL) {
        close(sockfd);
        return ENOMEM;
    }

    ret = sock_buff_create(sockfd, 8192, 65536,
                           &client_sock_buff_handler, client,
                           &client->sock);
    if (ret != 0) {
        free(client);
        close(sockfd);
        return ret;
    }

    uo_decompression_init(&client->decompression);
    client->decompressed_buffer = fifo_buffer_new(65536);
    if (client->decompressed_buffer == NULL) {
        uo_client_dispose(client);
        return ENOMEM;
    }

    client->handler = handler;
    client->handler_ctx = handler_ctx;

    *clientp = client;

    /* seed must be the first 4 bytes, and it must be flushed */
    uo_client_send(client, (unsigned char*)&seed, sizeof(seed));
    sock_buff_flush(client->sock);

    return 0;
}

void uo_client_dispose(struct uo_client *client) {
    fifo_buffer_free(client->decompressed_buffer);

    if (client->sock != NULL)
        sock_buff_dispose(client->sock);

    if (uo_client_is_aborted(client))
        evtimer_del(&client->abort_event);

    free(client);
}

void uo_client_send(struct uo_client *client,
                    const void *src, size_t length) {
    void *dest;
    size_t max_length;

    assert(client->sock != NULL || uo_client_is_aborted(client));
    assert(length > 0);

    if (uo_client_is_aborted(client))
        return;

#ifdef DUMP_CLIENT_SEND
    printf("sending to packet to server, length=%zu:\n", length);
    fhexdump(stdout, "  ", src, length);
    fflush(stdout);
#endif

    if (*(const unsigned char*)src == PCK_GameLogin)
        client->compression_enabled = 1;

    dest = fifo_buffer_write(client->sock->output, &max_length);
    if (dest == NULL || length > max_length) {
        fprintf(stderr, "output buffer full in uo_client_send()\n");
        uo_client_abort(client);
        return;
    }

    memcpy(dest, src, length);
    fifo_buffer_append(client->sock->output, length);
    sock_buff_flush(client->sock);
}
