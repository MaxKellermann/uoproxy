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

#ifndef __HANDLER_H
#define __HANDLER_H

#include <stddef.h>

struct connection;
struct linked_server;

/** what to do with the packet? */
typedef enum {
    /** forward the packet to the other communication partner */
    PA_ACCEPT = 0,

    /** drop the packet */
    PA_DROP,

    /** disconnect the endpoint from which this packet was received */
    PA_DISCONNECT,

    /** the endpoint has been deleted */
    PA_DELETED,
} packet_action_t;

struct client_packet_binding {
    unsigned char cmd;
    packet_action_t (*handler)(struct connection *c,
                               const void *data, size_t length);
};

struct server_packet_binding {
    unsigned char cmd;
    packet_action_t (*handler)(struct linked_server *ls,
                               const void *data, size_t length);
};

extern struct client_packet_binding server_packet_bindings[];
extern struct server_packet_binding client_packet_bindings[];

packet_action_t
handle_packet_from_server(struct client_packet_binding *bindings,
                          struct connection *c,
                          const void *data, size_t length);

packet_action_t
handle_packet_from_client(struct server_packet_binding *bindings,
                          struct linked_server *ls,
                          const void *data, size_t length);

#endif
