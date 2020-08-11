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

#pragma once

#include "World.hxx"

#include <event.h>

#define MAX_CHARACTERS 16

struct ClientVersion;

namespace UO {
class Client;
class ClientHandler;
}

struct StatefulClient {
    bool reconnecting = false, version_requested = false;
    struct event reconnect_event;

    UO::Client *client = nullptr;
    struct event ping_event;

    struct uo_fragment_character_info characters[MAX_CHARACTERS];
    unsigned num_characters = 0;

    uint32_t supported_features_flags = 0;

    unsigned char ping_request = 0, ping_ack = 0;

    World world;

    StatefulClient() noexcept;

    ~StatefulClient() noexcept {
        if (reconnecting)
            event_del(&reconnect_event);
    }

    StatefulClient(const StatefulClient &) = delete;
    StatefulClient &operator=(const StatefulClient &) = delete;

    bool IsInGame() const noexcept {
        return world.HasStart();
    }

    void Connect(int fd, const ClientVersion &version,
                 uint32_t seed,
                 UO::ClientHandler &handler);

    void Disconnect() noexcept;

    void SchedulePing() noexcept {
        static constexpr struct timeval tv{30, 0};
        event_add(&ping_event, &tv);
    }
};
