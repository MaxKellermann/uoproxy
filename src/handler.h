/*
 * uoproxy
 *
 * (c) 2005 Max Kellermann <max@duempel.org>
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

typedef enum {
    PA_ACCEPT = 0,
    PA_DROP,
    PA_DISCONNECT,
} packet_action_t;

typedef packet_action_t (*packet_handler_t)(struct connection *c,
                                            const void *data, size_t length);

struct packet_binding {
    unsigned char cmd;
    packet_handler_t handler;
};

extern struct packet_binding server_packet_bindings[];
extern struct packet_binding client_packet_bindings[];

packet_action_t handle_packet(struct packet_binding *bindings,
                              struct connection *c,
                              void *data, size_t length);

#endif
