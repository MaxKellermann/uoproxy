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

#include <sys/types.h>

#include "instance.h"
#include "connection.h"
#include "ioutil.h"

void instance_pre_select(struct instance *instance,
                         struct selectx *sx) {
    struct connection *c, *n;

    list_for_each_entry_safe(c, n, &instance->connections, siblings) {
        if (c->invalid) {
            list_del(&c->siblings);
            connection_delete(c);
        } else {
            connection_pre_select(c, sx);
        }
    }
}

void instance_post_select(struct instance *instance,
                          struct selectx *sx) {
    struct connection *c;

    list_for_each_entry(c, &instance->connections, siblings)
        connection_post_select(c, sx);
}

void instance_idle(struct instance *instance, time_t now) {
    struct connection *c;

    list_for_each_entry(c, &instance->connections, siblings)
        connection_idle(c, now);
}

void instance_schedule(struct instance *instance, time_t secs) {
    if (instance->tv.tv_sec >= secs)
        instance->tv = (struct timeval){
            .tv_sec = secs,
            .tv_usec = 0,
        };
}

