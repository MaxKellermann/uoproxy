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

#ifndef __RELAY_H
#define __RELAY_H

#define MAX_RELAYS 16

struct relay {
    u_int32_t auth_id;
    u_int32_t server_ip;
    u_int16_t server_port;
};

struct relay_list {
    struct relay relays[MAX_RELAYS];
    unsigned next;
};

static inline void relay_add(struct relay_list *list,
                             const struct relay *relay) {
    list->relays[list->next] = *relay;
    list->next = (list->next + 1) % MAX_RELAYS;
}

static inline const struct relay *relay_find(struct relay_list *list,
                                             u_int32_t auth_id) {
    unsigned i;

    for (i = 0; i < MAX_RELAYS; i++) {
        if (list->relays[i].auth_id == auth_id) {
            list->relays[i].auth_id = 0;
            return &list->relays[i];
        }
    }

    return NULL;
}

#endif
