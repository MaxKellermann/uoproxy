// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "event/net/ServerSocket.hxx"
#include "util/IntrusiveList.hxx"

struct Instance;
struct Connection;
struct LinkedServer;
namespace UO { struct CredentialsFragment; }

class Listener final : ServerSocket {
    Instance &instance;
    IntrusiveList<Connection> connections;

public:
    Listener(Instance &_instance, SocketAddress bind_address);
    ~Listener() noexcept;

    Connection *FindAttachConnection(const UO::CredentialsFragment &credentials) noexcept;
    Connection *FindAttachConnection(Connection &c) noexcept;

    LinkedServer *FindZombie(const struct uo_packet_game_login &game_login) noexcept;

private:
    // virtual methods from ServerSocket
    void OnAccept(UniqueSocketDescriptor fd, SocketAddress address) noexcept override;
    void OnAcceptError(std::exception_ptr error) noexcept override;
};
