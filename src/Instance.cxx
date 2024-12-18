// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

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

LinkedServer *
Instance::FindZombie(const struct uo_packet_game_login &game_login) noexcept
{
    for (auto &i : connections) {
        auto *zombie = i.FindZombie(game_login);
        if (zombie != nullptr)
            return zombie;
    }

    return nullptr;
}
