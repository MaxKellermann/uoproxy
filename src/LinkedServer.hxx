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

#include "util/IntrusiveList.hxx"
#include "Server.hxx"
#include "CVersion.hxx"

#include <event.h>

struct Connection;

namespace UO {
class Server;
}

struct LinkedServer final : IntrusiveListHook, UO::ServerHandler {
    Connection *connection;

    UO::Server *server = nullptr;
    bool welcome = false, attaching = false;

    struct client_version client_version;

    /** Razor_workaround support here: we save the charlist until
        the client says gamelogin, at which point we turn compression on in our
        emulated server and send a charlist. */
    bool expecting_reconnect, got_gamelogin;
    struct uo_packet_simple_character_list *enqueued_charlist;

    bool is_zombie; /**< zombie handling */
    struct event zombie_timeout; /**< zombies time out and auto-reap themselves
                                      after 5 seconds using this timer */
    uint32_t auth_id; /**< unique identifier for this linked_server used in
                           redirect handling to locate the zombied
                           linked_server */

    explicit LinkedServer(int fd)
        :server(uo_server_create(fd, *this))
    {
    }

    ~LinkedServer() noexcept;

    LinkedServer(const LinkedServer &) = delete;
    LinkedServer &operator=(const LinkedServer &) = delete;

    /* virtual methods from UO::ServerHandler */
    bool OnServerPacket(const void *data, size_t length) override;
    void OnServerDisconnect() noexcept override;
};
