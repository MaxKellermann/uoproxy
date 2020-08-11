/*
 * uoproxy
 *
 * Copyright 2005-2020 Max Kellermann <max.kellermann@gmail.com>
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

#include "Instance.hxx"
#include "Connection.hxx"
#include "Log.hxx"
#include "NetUtil.hxx"
#include "Config.hxx"

#include <unistd.h>
#include <errno.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#endif

static void
listener_event_callback(int fd, short, void *ctx)
{
    auto instance = (Instance *)ctx;
    struct sockaddr_storage sa;
    socklen_t sa_len;
    int remote_fd, ret;
    Connection *c;

    sa_len = sizeof(sa);
    remote_fd = accept(fd, (struct sockaddr*)&sa, &sa_len);
    if (remote_fd < 0) {
        if (errno != EAGAIN
#ifndef _WIN32
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

    instance->connections.push_front(*c);
}

void
instance_setup_server_socket(Instance *instance)
{
    instance->server_socket = setup_server_socket(instance->config.bind_address);

    event_set(&instance->server_socket_event, instance->server_socket,
              EV_READ|EV_PERSIST,
              listener_event_callback, instance);
    event_add(&instance->server_socket_event, nullptr);
}

Connection *
Instance::FindAttachConnection(const UO::CredentialsFragment &credentials) noexcept
{
    for (auto &i : connections)
        if (i.CanAttach() && credentials == i.credentials)
            return &i;

    return nullptr;
}

Connection *
Instance::FindAttachConnection(Connection &c) noexcept
{
    for (auto &i : connections)
        if (&i != &c && i.CanAttach() &&
            c.credentials == i.credentials &&
            c.server_index == i.server_index)
            return &i;

    return nullptr;
}
