/*
 * uoproxy
 * $Id$
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

#ifndef __CONNECTION_H
#define __CONNECTION_H

struct connection {
    /* linked list */
    struct connection *next;

    /* flags */
    int invalid;

    /* network addresses */
    u_int32_t local_ip, server_ip;
    u_int16_t local_port, server_port;

    /* sub-objects */
    struct uo_client *client;
    struct uo_server *server;
};

struct relay_list relays;

int connection_new(int server_socket,
                   u_int32_t local_ip, u_int16_t local_port,
                   u_int32_t server_ip, u_int16_t server_port,
                   struct connection **connectionp);

void connection_delete(struct connection *c);

void connection_invalidate(struct connection *c);

#endif
