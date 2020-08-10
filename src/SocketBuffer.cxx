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

#include "SocketBuffer.hxx"
#include "compiler.h"
#include "BufferedIO.hxx"
#include "FifoBuffer.hxx"
#include "Flush.hxx"
#include "Log.hxx"

#include <event.h>

#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>

struct sock_buff {
    struct pending_flush flush;

    int fd;

    struct event recv_event, send_event;

    struct fifo_buffer *input, *output;

    const struct sock_buff_handler *handler;
    void *handler_ctx;
};


/*
 * invoke wrappers
 *
 */

/**
 * @return false on error or if nothing was consumed
 */
static bool
sock_buff_invoke_data(struct sock_buff *sb)
{
    const void *data;
    size_t length;
    ssize_t nbytes;

    assert(sb->handler != nullptr);
    assert(sb->handler->data != nullptr);

    data = fifo_buffer_read(sb->input, &length);
    if (data == nullptr)
        return true;

    flush_begin();
    nbytes = sb->handler->data(data, length, sb->handler_ctx);
    flush_end();
    if (nbytes == 0)
        return false;

    fifo_buffer_consume(sb->input, (size_t)nbytes);
    return true;
}

static void
sock_buff_invoke_free(struct sock_buff *sb, int error)
{
    const struct sock_buff_handler *handler;

    assert(sb->handler != nullptr);
    assert(sb->fd >= 0);

    event_del(&sb->recv_event);
    event_del(&sb->send_event);

    flush_del(&sb->flush);

    handler = sb->handler;
    sb->handler = nullptr;

    handler->free(error, sb->handler_ctx);
}


/*
 * flush
 *
 */

/**
 * Try to flush the output buffer.  Note that this function will not
 * trigger the free() callback.
 *
 * @return true on success, false on i/o error (see errno)
 */
static bool
sock_buff_flush(struct sock_buff *sb)
{
    ssize_t nbytes;

    nbytes = write_from_buffer(sb->fd, sb->output);
    if (nbytes == -2)
        return true;

    if (nbytes < 0)
        return false;

    return true;
}

static void
sock_buff_flush_callback(struct pending_flush *flush)
{
    struct sock_buff *sb = (struct sock_buff *)flush;

    if (!sock_buff_flush(sb))
        return;

    if (fifo_buffer_empty(sb->output))
        event_del(&sb->send_event);
}


/*
 * libevent callback function
 *
 */

static void
sock_buff_recv_callback(int fd, short event, void *ctx)
{
    auto sb = (struct sock_buff *)ctx;

    (void)event;

    assert(fd == sb->fd);

    ssize_t nbytes = read_to_buffer(fd, sb->input, 65536);
    if (nbytes > 0) {
        if (!sock_buff_invoke_data(sb))
            return;
    } else if (nbytes == 0) {
        sock_buff_invoke_free(sb, 0);
        return;
    } else if (nbytes == -1) {
        sock_buff_invoke_free(sb, errno);
        return;
    }

    if (fifo_buffer_full(sb->input))
        event_del(&sb->recv_event);
}

static void
sock_buff_send_callback(int fd, short event, void *ctx)
{
    auto sb = (struct sock_buff *)ctx;

    (void)fd;
    (void)event;

    assert(fd == sb->fd);

    if (!sock_buff_flush(sb)) {
        sock_buff_invoke_free(sb, errno);
        return;
    }

    if (fifo_buffer_empty(sb->output))
        event_del(&sb->send_event);
}


/*
 * methods
 *
 */

void *
sock_buff_write(struct sock_buff *sb, size_t *max_length_r)
{
    return fifo_buffer_write(sb->output, max_length_r);
}

void
sock_buff_append(struct sock_buff *sb, size_t length)
{
    fifo_buffer_append(sb->output, length);

    event_add(&sb->send_event, nullptr);
    flush_add(&sb->flush);
}

bool
sock_buff_send(struct sock_buff *sb, const void *data, size_t length)
{
    void *dest;
    size_t max_length;

    dest = sock_buff_write(sb, &max_length);
    if (dest == nullptr || length > max_length)
        return false;

    memcpy(dest, data, length);
    sock_buff_append(sb, length);
    return true;
}

uint32_t sock_buff_sockname(const struct sock_buff *sb)
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int ret = getsockname(sb->fd, (struct sockaddr *)&addr, &len);
    if (ret) {
        log_errno("getsockname()");
        return 0;
    }
    return addr.sin_addr.s_addr;
}

uint16_t sock_buff_port(const struct sock_buff *sb)
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int ret = getsockname(sb->fd, (struct sockaddr *)&addr, &len);
    if (ret) {
        log_errno("getsockname()");
        return 0;
    }
    return addr.sin_port;
}

/*
 * constructor and destructor
 *
 */

struct sock_buff *
sock_buff_create(int fd, size_t input_max,
                 size_t output_max,
                 const struct sock_buff_handler *handler,
                 void *handler_ctx)
{
    struct sock_buff *sb;

    assert(handler != nullptr);
    assert(handler->data != nullptr);
    assert(handler->free != nullptr);

    sb = (struct sock_buff*)malloc(sizeof(*sb));
    if (sb == nullptr)
        return nullptr;

    flush_init(&sb->flush, sock_buff_flush_callback);

    sb->fd = fd;

    event_set(&sb->recv_event, fd, EV_READ|EV_PERSIST,
              sock_buff_recv_callback, sb);
    event_set(&sb->send_event, fd, EV_WRITE|EV_PERSIST,
              sock_buff_send_callback, sb);

    sb->input = fifo_buffer_new(input_max);
    if (sb->input == nullptr) {
        free(sb);
        return nullptr;
    }

    sb->output = fifo_buffer_new(output_max);
    if (sb->output == nullptr) {
        fifo_buffer_free(sb->input);
        free(sb);
        return nullptr;
    }

    sb->handler = handler;
    sb->handler_ctx = handler_ctx;

    event_add(&sb->recv_event, nullptr);

    return sb;
}

void sock_buff_dispose(struct sock_buff *sb) {
    assert(sb->fd >= 0);

    event_del(&sb->recv_event);
    event_del(&sb->send_event);

    flush_del(&sb->flush);

    close(sb->fd);

    fifo_buffer_free(sb->input);
    fifo_buffer_free(sb->output);

    free(sb);
}
