// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Instance.hxx"
#include "Connection.hxx"
#include "Log.hxx"
#include "NetUtil.hxx"
#include "Config.hxx"
#include "net/SocketError.hxx"

static void
listener_event_callback(int, short, void *ctx)
{
    auto instance = (Instance *)ctx;
    Connection *c;

    auto remote_fd = instance->server_socket.AcceptNonBlock();
    if (!remote_fd.IsDefined()) {
        if (!IsSocketErrorAcceptWouldBlock(GetSocketError()))
            log_errno("accept() failed");
        return;
    }

    connection_new(instance, std::move(remote_fd), &c);
    instance->connections.push_front(*c);
}

void
instance_setup_server_socket(Instance *instance)
{
    instance->server_socket = setup_server_socket(instance->config.bind_address);

    event_set(&instance->server_socket_event, instance->server_socket.Get(),
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
