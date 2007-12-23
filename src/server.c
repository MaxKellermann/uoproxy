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

#include "server.h"
#include "sockbuff.h"
#include "compression.h"
#include "packets.h"
#include "dump.h"
#include "fifo-buffer.h"
#include "log.h"
#include "compiler.h"
#include "socket-util.h"

#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <event.h>

struct uo_server {
    struct sock_buff *sock;
    uint32_t seed;
    int compression_enabled;

    const struct uo_server_handler *handler;
    void *handler_ctx;

    struct event abort_event;
};

static void
uo_server_invoke_free(struct uo_server *server)
{
    const struct uo_server_handler *handler;

    assert(server->handler != NULL);

    handler = server->handler;
    server->handler = NULL;

    handler->free(server->handler_ctx);
}

static void
uo_server_abort_event_callback(int fd __attr_unused,
                               short event __attr_unused,
                               void *ctx)
{
    struct uo_server *server = ctx;

    uo_server_invoke_free(server);
}

static void
uo_server_abort(struct uo_server *server)
{
    struct timeval tv = {
        .tv_sec = 0,
        .tv_usec = 0,
    };

    /* this is a trick to delay the destruction of this object until
       everything is done */
    evtimer_set(&server->abort_event,
                uo_server_abort_event_callback, server);
    evtimer_add(&server->abort_event, &tv);
}

static int
uo_server_is_aborted(struct uo_server *server)
{
    return server->abort_event.ev_events != 0;
}

static ssize_t
server_packets_from_buffer(struct uo_server *server,
                           const unsigned char *data, size_t length)
{
    size_t consumed = 0, packet_length;
    int ret;

    while (length > 0) {
        packet_length = get_packet_length(data, length);
        if (packet_length == PACKET_LENGTH_INVALID) {
            fprintf(stderr, "malformed packet from server:\n");
            fhexdump(stderr, "  ", data, length);
            fflush(stderr);
            uo_server_abort(server);
            return 0;
        }

#ifdef DUMP_HEADERS
        printf("from client: 0x%02x length=%zu\n",
               data[0], packet_length);
#endif

        if (packet_length == 0 || packet_length > length)
            break;

#ifdef DUMP_CLIENT_RECEIVE
        fhexdump(stdout, "  ", data, packet_length);
        fflush(stdout);
#endif

        ret = server->handler->packet(data, packet_length,
                                      server->handler_ctx);
        if (ret < 0)
            return ret;

        consumed += packet_length;
        data += packet_length;
        length -= packet_length;
    }

    return (ssize_t)consumed;
}

static ssize_t
server_sock_buff_data(const void *data0, size_t length, void *ctx)
{
    const unsigned char *data = data0;
    struct uo_server *server = ctx;
    size_t consumed = 0;
    ssize_t nbytes;

    if (server->seed == 0) {
        /* the first packet from a client is the seed, 4 bytes without
           header */
        if (length < 4)
            return 0;

        server->seed = *(const uint32_t*)(data + consumed);
        if (server->seed == 0) {
            log(2, "zero seed from client\n");
            uo_server_abort(server);
            return 0;
        }

        consumed += sizeof(uint32_t);
    }

    nbytes = server_packets_from_buffer(server,
                                        data + consumed, length - consumed);
    if (nbytes < 0)
        return nbytes;

    return consumed + (size_t)nbytes;
}

static void
server_sock_buff_free(int error, void *ctx)
{
    struct uo_server *server = ctx;

    if (error == 0)
        log(2, "client closed the connection\n");
    else
        log_error("error during communication with client", error);

    sock_buff_dispose(server->sock);
    server->sock = NULL;

    uo_server_invoke_free(server);
}

struct sock_buff_handler server_sock_buff_handler = {
    .data = server_sock_buff_data,
    .free = server_sock_buff_free,
};

int uo_server_create(int sockfd,
                     const struct uo_server_handler *handler,
                     void *handler_ctx,
                     struct uo_server **serverp) {
    int ret;
    struct uo_server *server;

    assert(handler != NULL);
    assert(handler->packet != NULL);
    assert(handler->free != NULL);

    ret = socket_set_nonblock(sockfd, 1);
    if (ret < 0)
        return errno;

    server = (struct uo_server*)calloc(1, sizeof(*server));
    if (server == NULL)
        return ENOMEM;

    ret = sock_buff_create(sockfd, 8192, 65536,
                           &server_sock_buff_handler, server,
                           &server->sock);
    if (ret != 0) {
        free(server);
        return ret;
    }

    server->handler = handler;
    server->handler_ctx = handler_ctx;

    *serverp = server;

    return 0;
}

void uo_server_dispose(struct uo_server *server) {
    if (server->sock != NULL)
        sock_buff_dispose(server->sock);

    if (uo_server_is_aborted(server))
        evtimer_del(&server->abort_event);

    free(server);
}

uint32_t uo_server_seed(const struct uo_server *server) {
    return server->seed;
}

void uo_server_send(struct uo_server *server,
                    const void *src, size_t length) {
    void *dest;
    size_t max_length;

    assert(server->sock != NULL || uo_server_is_aborted(server));
    assert(length > 0);
    assert(get_packet_length(src, length) == length);

    if (uo_server_is_aborted(server))
        return;

#ifdef DUMP_SERVER_SEND
    printf("sending to packet to client, length=%zu:\n", length);
    fhexdump(stdout, "  ", src, length);
    fflush(stdout);
#endif

    dest = fifo_buffer_write(server->sock->output, &max_length);
    if (dest == NULL) {
        fprintf(stderr, "output buffer full in uo_server_send()\n");
        uo_server_abort(server);
        return;
    }

    if (server->compression_enabled) {
        ssize_t nbytes;

        nbytes = uo_compress(dest, max_length, src, length);
        if (nbytes < 0) {
            fprintf(stderr, "uo_compress() failed\n");
            uo_server_abort(server);
            return;
        }

        fifo_buffer_append(server->sock->output, (size_t)nbytes);
    } else {
        if (length > max_length) {
            fprintf(stderr, "output buffer full in uo_server_send()\n");
            uo_server_abort(server);
            return;
        }

        memcpy(dest, src, length);
        fifo_buffer_append(server->sock->output, length);
    }

    sock_buff_flush(server->sock);
}
