// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

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
     * this callback, and the method has to invoke this function.
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
