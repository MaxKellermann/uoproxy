// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "util/IntrusiveList.hxx"

#include <event.h>

struct Config;
struct Connection;
struct LinkedServer;
namespace UO { struct CredentialsFragment; }

struct Instance {
    /* configuration */
    const Config &config;

    /* state */

    struct event sigterm_event, sigint_event, sigquit_event;
    bool should_exit = false;

    int server_socket;
    struct event server_socket_event;

    IntrusiveList<Connection> connections;

    explicit Instance(Config &_config) noexcept
        :config(_config) {}

    Connection *FindAttachConnection(const UO::CredentialsFragment &credentials) noexcept;
    Connection *FindAttachConnection(Connection &c) noexcept;

    LinkedServer *FindZombie(const struct uo_packet_game_login &game_login) noexcept;
};

void
instance_setup_server_socket(Instance *instance);
