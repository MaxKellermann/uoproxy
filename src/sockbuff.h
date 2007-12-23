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

#ifndef __SOCKBUFF_H
#define __SOCKBUFF_H

#include "fifo-buffer.h"

#include <sys/types.h>
#include <event.h>

struct sock_buff_handler {
    /**
     * Data is available.
     *
     * @return 0, or -1 if the sock_buff has been closed within the
     * function
     */
    int (*data)(void *ctx);

    /**
     * The socket has been closed due to an error or because the peer
     * closed his side.  sock_buff_dispose() does not trigger this
     * callback, and the callee has to invoke this function.
     */
    void (*free)(int error, void *ctx);
};

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
                     struct sock_buff **sbp);
void sock_buff_dispose(struct sock_buff *sb);

void
sock_buff_event_setup(struct sock_buff *sb);

int sock_buff_flush(struct sock_buff *sb);

#endif
