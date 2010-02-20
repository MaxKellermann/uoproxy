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

#ifndef __INSTANCE_H
#define __INSTANCE_H

#include "list.h"

#include <stdbool.h>
#include <sys/types.h>
#include <event.h>

#ifdef WIN32
#define DISABLE_DAEMON_CODE
#endif

struct instance {
    /* configuration */
    struct config *config;

    /* state */

    struct event sigterm_event, sigint_event, sigquit_event;
    bool should_exit;

    int server_socket;
    struct event server_socket_event;

    struct list_head connections;

    struct timeval tv;
};

void
instance_setup_server_socket(struct instance *instance);

#ifdef DISABLE_DAEMON_CODE
static inline void
instance_daemonize(struct instance *instance)
{
    (void)instance;
}
#else
void instance_daemonize(struct instance *instance);
#endif

#endif
