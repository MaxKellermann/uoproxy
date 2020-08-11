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

#ifndef __CLIENT_H
#define __CLIENT_H

#include "PVersion.hxx"

#include <stdint.h>
#include <stddef.h>

struct uo_packet_seed;

namespace UO {

class Client;

class ClientHandler {
public:
    /**
     * A packet has been received.
     *
     * @return false if this object has been closed within the
     * function
     */
    virtual bool OnClientPacket(const void *data, size_t length) = 0;

    /**
     * The connection has been closed due to an error or because the
     * peer closed his side.  uo_client_dispose() does not trigger
     * this callback, and the callee has to invoke this function.
     */
    virtual void OnClientDisconnect() noexcept = 0;
};

} // namespace UO

UO::Client *
uo_client_create(int fd, uint32_t seed,
                 const struct uo_packet_seed *seed6,
                 UO::ClientHandler &handler);

void uo_client_dispose(UO::Client *client);

void
uo_client_set_protocol(UO::Client *client,
                       enum protocol_version protocol_version);

void uo_client_send(UO::Client *client,
                    const void *src, size_t length);

#endif
