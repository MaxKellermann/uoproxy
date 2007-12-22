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

#ifndef __CLIENT_H
#define __CLIENT_H

struct addrinfo;
struct uo_client;

struct uo_client_handler {
    int (*packet)(void *data, size_t length, void *ctx);
    void (*free)(void *ctx);
};

int uo_client_create(const struct addrinfo *server_address,
                     u_int32_t seed,
                     const struct uo_client_handler *handler,
                     void *handler_ctx,
                     struct uo_client **clientp);
void uo_client_dispose(struct uo_client *client);

int uo_client_fileno(const struct uo_client *client);

void uo_client_send(struct uo_client *client,
                    const void *src, size_t length);

#endif
