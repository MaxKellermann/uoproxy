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

#ifndef __HANDLER_H
#define __HANDLER_H

#include <stddef.h>

struct Connection;
struct LinkedServer;

/** what to do with the packet? */
enum class PacketAction {
    /** forward the packet to the other communication partner */
    ACCEPT,

    /** drop the packet */
    DROP,

    /** disconnect the endpoint from which this packet was received */
    DISCONNECT,

    /** the endpoint has been deleted */
    DELETED,
};

struct client_packet_binding {
    unsigned char cmd;
    PacketAction (*handler)(Connection *c,
                            const void *data, size_t length);
};

struct server_packet_binding {
    unsigned char cmd;
    PacketAction (*handler)(LinkedServer *ls,
                            const void *data, size_t length);
};

extern const struct client_packet_binding server_packet_bindings[];
extern const struct server_packet_binding client_packet_bindings[];

PacketAction
handle_packet_from_server(const struct client_packet_binding *bindings,
                          Connection *c,
                          const void *data, size_t length);

PacketAction
handle_packet_from_client(const struct server_packet_binding *bindings,
                          LinkedServer *ls,
                          const void *data, size_t length);

#endif
