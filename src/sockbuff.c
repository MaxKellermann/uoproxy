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

#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

int sock_buff_create(int fd, size_t input_max,
                     size_t output_max,
                     const struct sock_buff_handler *handler,
                     void *handler_ctx,
                     struct sock_buff **sbp) {
    struct sock_buff *sb;
    int ret;

    assert(handler != NULL);
    assert(handler->data != NULL);
    assert(handler->free != NULL);

    sb = (struct sock_buff*)malloc(sizeof(*sb));
    if (sb == NULL)
        return -ENOMEM;

    sb->fd = fd;
    sb->event.ev_events = 0;

    ret = fifo_buffer_new(input_max, &sb->input);
    if (ret < 0) {
        free(sb);
        return -ENOMEM;
    }

    ret = fifo_buffer_new(output_max, &sb->output);
    if (ret < 0) {
        fifo_buffer_delete(&sb->input);
        free(sb);
        return -ENOMEM;
    }

    sb->handler = handler;
    sb->handler_ctx = handler_ctx;

    sock_buff_event_setup(sb);

    *sbp = sb;

    return 0;
}

static void
sock_buff_invoke_free(struct sock_buff *sb, int error)
{
    const struct sock_buff_handler *handler;

    assert(sb->handler != NULL);
    assert(sb->fd >= 0);

    handler = sb->handler;
    sb->handler = NULL;

    handler->free(error, sb->handler_ctx);
}

void sock_buff_dispose(struct sock_buff *sb) {
    assert(sb->fd >= 0);

    event_del(&sb->event);
    close(sb->fd);

    fifo_buffer_delete(&sb->input);
    fifo_buffer_delete(&sb->output);
    free(sb);
}

int sock_buff_flush(struct sock_buff *sb) {
    const unsigned char *p;
    size_t length = 0;
    ssize_t nbytes;

    p = fifo_buffer_read(sb->output, &length);
    if (p == NULL)
        return 0;

    assert(length > 0);

    nbytes = write(sb->fd, p, length);
    if (nbytes < 0) {
        int save_errno = errno;
        sock_buff_invoke_free(sb, errno);
        return -save_errno;
    }

    fifo_buffer_consume(sb->output, (size_t)nbytes);
    return 0;
}

static void
sock_buff_event_callback(int fd, short event, void *ctx)
{
    struct sock_buff *sb = ctx;
    int ret;

    assert(fd == sb->fd);

    if (event & EV_READ) {
        void *p;
        size_t max_length;
        ssize_t nbytes;

        p = fifo_buffer_write(sb->input, &max_length);
        assert(p != NULL);

        nbytes = read(sb->fd, p, max_length);
        if (nbytes < 0) {
            perror("failed to read");
            sock_buff_invoke_free(sb, errno);
            return;
        }

        if (nbytes == 0) {
            sock_buff_invoke_free(sb, 0);
            return;
        }

        fifo_buffer_append(sb->input, (size_t)nbytes);

        ret = sb->handler->data(sb->handler_ctx);
        if (ret < 0)
            return;
    }

    if (event & EV_WRITE) {
        ret = sock_buff_flush(sb);
        if (ret < 0)
            perror("failed to write");
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

    if (sb->event.ev_events != 0)
        event_del(&sb->event);

    if (event != 0) {
        event_set(&sb->event, sb->fd, event, sock_buff_event_callback, sb);
        event_add(&sb->event, NULL);
    }
}
