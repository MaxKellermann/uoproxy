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

#include "instance.h"
#include "connection.h"
#include "compiler.h"
#include "log.h"
#include "netutil.h"
#include "config.h"

#include <unistd.h>
#include <errno.h>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#endif

static void
listener_event_callback(int fd, short event __attr_unused, void *ctx)
{
    struct instance *instance = ctx;
    struct sockaddr_storage sa;
    socklen_t sa_len;
    int remote_fd, ret;
    struct connection *c;

    sa_len = sizeof(sa);
    remote_fd = accept(fd, (struct sockaddr*)&sa, &sa_len);
    if (remote_fd < 0) {
        if (errno != EAGAIN
#ifndef WIN32
            && errno != EWOULDBLOCK
#endif
            )
            log_errno("accept() failed");
        return;
    }

    ret = connection_new(instance, remote_fd, &c);
    if (ret != 0) {
        log_error("connection_new() failed", ret);
        close(remote_fd);
        return;
    }

    list_add(&c->siblings, &instance->connections);
}

void
instance_setup_server_socket(struct instance *instance)
{
    instance->server_socket = setup_server_socket(instance->config->bind_address);

    event_set(&instance->server_socket_event, instance->server_socket,
              EV_READ|EV_PERSIST,
              listener_event_callback, instance);
    event_add(&instance->server_socket_event, NULL);
}
