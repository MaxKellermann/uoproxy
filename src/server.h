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

#ifndef __SERVER_H
#define __SERVER_H

#include "pversion.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

struct uo_server;

struct uo_server_handler {
    /**
     * A packet has been received.
     *
     * @return 0, or -1 if this object has been closed within the
     * function
     */
    int (*packet)(const void *data, size_t length, void *ctx);

    /**
     * The connection has been closed due to an error or because the
     * peer closed his side.  uo_server_dispose() does not trigger
     * this callback, and the callee has to invoke this function.
     */
    void (*free)(void *ctx);
};

int uo_server_create(int sockfd,
                     const struct uo_server_handler *handler,
                     void *handler_ctx,
                     struct uo_server **serverp);
void uo_server_dispose(struct uo_server *server);

uint32_t uo_server_seed(const struct uo_server *server);

void
uo_server_set_protocol(struct uo_server *server,
                       enum protocol_version protocol_version);

void uo_server_set_compression(struct uo_server *server, bool compression);

void uo_server_send(struct uo_server *server,
                    const void *src, size_t length);


/** @return ip address, in network byte order, of our uo server socket
            (= connection to client) */
uint32_t uo_server_getsockname(const struct uo_server *server);
/** @return port, in network byte order, of our uo server socket
            (= connection to client) */
uint16_t uo_server_getsockport(const struct uo_server *server);

/* utilities */

void uo_server_speak_ascii(struct uo_server *server,
                           uint32_t serial,
                           int16_t graphic,
                           uint8_t type,
                           uint16_t hue, uint16_t font,
                           const char *name,
                           const char *text);

void uo_server_speak_console(struct uo_server *server,
                             const char *text);

#endif
