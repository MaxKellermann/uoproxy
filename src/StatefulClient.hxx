// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "CVersion.hxx"
#include "World.hxx"

#include <event.h>

class UniqueSocketDescriptor;

namespace UO {
class Client;
class ClientHandler;
}

struct StatefulClient {
    bool reconnecting = false, version_requested = false;

    UO::Client *client = nullptr;
    struct event ping_event;

    ClientVersion version;

    /**
     * The most recent game server list packet received from the
     * server.  This does not get cleared when reconnecting, but gets
     * overwritten when a new one is received.
     */
    VarStructPtr<struct uo_packet_server_list> server_list;

    /**
     * The most recent character list packet received from the server.
     * This does not get cleared when reconnecting, but gets
     * overwritten when a new one is received.
     */
    VarStructPtr<struct uo_packet_simple_character_list> char_list;

    uint32_t supported_features_flags = 0;

    unsigned char ping_request = 0, ping_ack = 0;

    World world;

    StatefulClient() noexcept;

    StatefulClient(const StatefulClient &) = delete;
    StatefulClient &operator=(const StatefulClient &) = delete;

    bool IsInGame() const noexcept {
        return world.HasStart();
    }

    bool IsConnected() const noexcept {
        return client != nullptr;
    }

    void Connect(UniqueSocketDescriptor &&s,
                 uint32_t seed, bool for_game_login,
                 UO::ClientHandler &handler);

    void Disconnect() noexcept;

    void SchedulePing() noexcept {
        static constexpr struct timeval tv{30, 0};
        event_add(&ping_event, &tv);
    }
};
