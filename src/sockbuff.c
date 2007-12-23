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

#include "sockbuff.h"
#include "compiler.h"
#include "buffered-io.h"

#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

struct sock_buff {
    int fd;
    struct event event;

    fifo_buffer_t input, output;

    const struct sock_buff_handler *handler;
    void *handler_ctx;
};

int sock_buff_create(int fd, size_t input_max,
                     size_t output_max,
                     const struct sock_buff_handler *handler,
                     void *handler_ctx,
                     struct sock_buff **sbp) {
    struct sock_buff *sb;

    assert(handler != NULL);
    assert(handler->data != NULL);
    assert(handler->free != NULL);

    sb = (struct sock_buff*)malloc(sizeof(*sb));
    if (sb == NULL)
        return ENOMEM;

    sb->fd = fd;
    sb->event.ev_events = 0;

    sb->input = fifo_buffer_new(input_max);
    if (sb->input == NULL) {
        free(sb);
        return ENOMEM;
    }

    sb->output = fifo_buffer_new(output_max);
    if (sb->output == NULL) {
        fifo_buffer_free(sb->input);
        free(sb);
        return ENOMEM;
    }

    sb->handler = handler;
    sb->handler_ctx = handler_ctx;

    sock_buff_event_setup(sb);

    *sbp = sb;

    return 0;
}

static int
sock_buff_invoke_data(struct sock_buff *sb)
{
    const void *data;
    size_t length;
    ssize_t nbytes;

    assert(sb->handler != NULL);
    assert(sb->handler->data != NULL);

    data = fifo_buffer_read(sb->input, &length);
    if (data == NULL)
        return 0;

    nbytes = sb->handler->data(data, length, sb->handler_ctx);
    if (nbytes < 0)
        return -1;

    fifo_buffer_consume(sb->input, (size_t)nbytes);
    return 0;
}

static void
sock_buff_invoke_free(struct sock_buff *sb, int error)
{
    const struct sock_buff_handler *handler;

    assert(sb->handler != NULL);
    assert(sb->fd >= 0);

    if (sb->event.ev_events != 0) {
        event_del(&sb->event);
        sb->event.ev_events = 0;
    }

    handler = sb->handler;
    sb->handler = NULL;

    handler->free(error, sb->handler_ctx);
}

void sock_buff_dispose(struct sock_buff *sb) {
    assert(sb->fd >= 0);

    if (sb->event.ev_events != 0) {
        event_del(&sb->event);
        sb->event.ev_events = 0;
    }

    close(sb->fd);

    fifo_buffer_free(sb->input);
    fifo_buffer_free(sb->output);
    free(sb);
}

int sock_buff_flush(struct sock_buff *sb) {
    ssize_t nbytes;

    nbytes = write_from_buffer(sb->fd, sb->output);
    if (nbytes == -2)
        return 0;

    if (nbytes < 0)
        return -1;

    sock_buff_event_setup(sb);

    return 0;
}

static void
sock_buff_event_callback(int fd, short event, void *ctx)
{
    struct sock_buff *sb = ctx;
    int ret;

    assert(fd == sb->fd);

    if (event & EV_READ) {
        ssize_t nbytes;

        nbytes = read_to_buffer(sb->fd, sb->input, 65536);
        if (nbytes == -1) {
            sock_buff_invoke_free(sb, errno);
            return;
        } else if (nbytes > 0) {
            ret = sock_buff_invoke_data(sb);
            if (ret < 0)
                return;
        }
    }

    if (event & EV_WRITE) {
        ret = sock_buff_flush(sb);
        if (ret < 0) {
            sock_buff_invoke_free(sb, errno);
            return;
        }
    }

    sock_buff_event_setup(sb);
}

void
sock_buff_event_setup(struct sock_buff *sb)
{
    short event = EV_PERSIST;

    if (!fifo_buffer_full(sb->input))
        event |= EV_READ;

    if (!fifo_buffer_empty(sb->output))
        event |= EV_WRITE;

    if (sb->event.ev_events == event)
        return;

    if (sb->event.ev_events != 0) {
        event_del(&sb->event);
        sb->event.ev_events = 0;
    }

    if (event != 0) {
        event_set(&sb->event, sb->fd, event, sock_buff_event_callback, sb);
        event_add(&sb->event, NULL);
    }
}

void *
sock_buff_write(struct sock_buff *sb, size_t *max_length_r)
{
    return fifo_buffer_write(sb->output, max_length_r);
}

void
sock_buff_append(struct sock_buff *sb, size_t length)
{
    fifo_buffer_append(sb->output, length);
    sock_buff_flush(sb);
}

void
sock_buff_send(struct sock_buff *sb, const void *data, size_t length)
{
    void *dest;
    size_t max_length;

    dest = fifo_buffer_write(sb->output, &max_length);
    if (dest == NULL || length > max_length) {
        sock_buff_invoke_free(sb, ENOSPC);
        return;
    }

    memcpy(dest, data, length);

    fifo_buffer_append(sb->output, length);

    sock_buff_flush(sb);
}
