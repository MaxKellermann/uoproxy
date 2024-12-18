// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "event/Loop.hxx"
#include "event/ShutdownListener.hxx"
#include "event/net/ServerSocket.hxx"
#include "net/UniqueSocketDescriptor.hxx"

#include <forward_list>

struct Config;
class Listener;
struct Connection;
struct LinkedServer;
namespace UO { struct CredentialsFragment; }

struct Instance {
    /* configuration */
    const Config &config;

    /* state */

    EventLoop event_loop;
    ShutdownListener shutdown_listener{event_loop, BIND_THIS_METHOD(OnShutdown)};

    std::forward_list<Listener> listeners;

    explicit Instance(Config &_config) noexcept;
    ~Instance() noexcept;

    Connection *FindAttachConnection(const UO::CredentialsFragment &credentials) noexcept;
    Connection *FindAttachConnection(Connection &c) noexcept;

    LinkedServer *FindZombie(const struct uo_packet_game_login &game_login) noexcept;

private:
	void OnShutdown() noexcept;
};

void
instance_setup_server_socket(Instance *instance);
