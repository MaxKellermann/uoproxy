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

#ifndef __CLIENT_H
#define __CLIENT_H

#include "pversion.h"

#include <stdint.h>
#include <stddef.h>

struct uo_client;
struct uo_packet_seed;

struct uo_client_handler {
    /**
     * A packet has been received.
     *
     * @return 0, or -1 if this object has been closed within the
     * function
     */
    int (*packet)(const void *data, size_t length, void *ctx);

    /**
     * The connection has been closed due to an error or because the
     * peer closed his side.  uo_client_dispose() does not trigger
     * this callback, and the callee has to invoke this function.
     */
    void (*free)(void *ctx);
};

int
uo_client_create(int fd, uint32_t seed,
                 const struct uo_packet_seed *seed6,
                 const struct uo_client_handler *handler,
                 void *handler_ctx,
                 struct uo_client **clientp);

void uo_client_dispose(struct uo_client *client);

void
uo_client_set_protocol(struct uo_client *client,
                       enum protocol_version protocol_version);

void uo_client_send(struct uo_client *client,
                    const void *src, size_t length);

#endif
