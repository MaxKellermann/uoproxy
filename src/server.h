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

#ifndef __SERVER_H
#define __SERVER_H

struct selectx;

struct uo_server;

int uo_server_create(int sockfd, struct uo_server **serverp);
void uo_server_dispose(struct uo_server *server);

int uo_server_alive(const struct uo_server *server);

int uo_server_fileno(const struct uo_server *server);

u_int32_t uo_server_seed(const struct uo_server *server);

void uo_server_pre_select(struct uo_server *server,
                          struct selectx *sx);
int uo_server_post_select(struct uo_server *server,
                          struct selectx *sx);

void *uo_server_peek(struct uo_server *server, size_t *lengthp);

void uo_server_shift(struct uo_server *server, size_t nbytes);

void uo_server_send(struct uo_server *server,
                    const void *src, size_t length);


/* utilities */

void uo_server_speak_ascii(struct uo_server *server,
                           u_int32_t serial,
                           int16_t graphic,
                           u_int8_t type,
                           u_int16_t hue, u_int16_t font,
                           const char *name,
                           const char *text);

void uo_server_speak_console(struct uo_server *server,
                             const char *text);

#endif
