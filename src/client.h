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

#ifndef __CLIENT_H
#define __CLIENT_H

struct selectx;

struct uo_client;

int uo_client_create(uint32_t ip, uint16_t port, struct uo_client **clientp);
void uo_client_dispose(struct uo_client *client);

int uo_client_alive(const struct uo_client *client);

void uo_client_pre_select(struct uo_client *client,
                          struct selectx *sx);
int uo_client_post_select(struct uo_client *client,
                          struct selectx *sx);

unsigned char *uo_client_receive(struct uo_client *client,
                                 unsigned char *dest, size_t *lengthp);

void uo_client_send(struct uo_client *client,
                    const unsigned char *src, size_t length);

#endif