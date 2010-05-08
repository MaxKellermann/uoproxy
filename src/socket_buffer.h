/*
 * uoproxy
 *
 * (c) 2005-2010 Max Kellermann <max@duempel.org>
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

#ifndef UOPROXY_SOCKET_BUFFER_H
#define UOPROXY_SOCKET_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct sock_buff_handler {
    /**
     * Data is available.
     *
     * @return the number of bytes consumed, or 0 if the sock_buff
     * has been closed within the function
     */
    size_t (*data)(const void *data, size_t length, void *ctx);

    /**
     * The socket has been closed due to an error or because the peer
     * closed his side.  sock_buff_dispose() does not trigger this
     * callback, and the callee has to invoke this function.
     */
    void (*free)(int error, void *ctx);
};

struct sock_buff;

struct sock_buff *
sock_buff_create(int fd, size_t input_max,
                 size_t output_max,
                 const struct sock_buff_handler *handler,
                 void *handler_ctx);

void sock_buff_dispose(struct sock_buff *sb);

void *
sock_buff_write(struct sock_buff *sb, size_t *max_length_r);

void
sock_buff_append(struct sock_buff *sb, size_t length);

/**
 * @return true on success, false if there is no more room in the
 * output buffer
 */
bool
sock_buff_send(struct sock_buff *sb, const void *data, size_t length);

/**
 * @return the 32-bit internet address of the socket buffer's fd, in
 * network byte order
 */
uint32_t sock_buff_sockname(const struct sock_buff *sb);

/**
 * @return the 16-bit port ofthe socket buffer's fd, in network byte
 * order
 */
uint16_t sock_buff_port(const struct sock_buff *sb);

#endif
