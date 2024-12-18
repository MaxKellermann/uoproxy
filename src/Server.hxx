// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "PVersion.hxx"

#include <stdint.h>
#include <stddef.h>

namespace UO {

class Server;

class ServerHandler {
public:
    /**
     * A packet has been received.
     *
     * @return false if this object has been closed within the
     * function
     */
    virtual bool OnServerPacket(const void *data, size_t length) = 0;

    /**
     * The connection has been closed due to an error or because the
     * peer closed his side.  uo_server_dispose() does not trigger
     * this callback, and the method has to invoke this function.
     */
    virtual void OnServerDisconnect() noexcept = 0;
};

} // namespace UO

UO::Server *
uo_server_create(int sockfd,
                 UO::ServerHandler &handler);
void uo_server_dispose(UO::Server *server);

uint32_t uo_server_seed(const UO::Server *server);

void
uo_server_set_protocol(UO::Server *server,
                       enum protocol_version protocol_version);

void uo_server_set_compression(UO::Server *server, bool compression);

void uo_server_send(UO::Server *server,
                    const void *src, size_t length);


/** @return ip address, in network byte order, of our uo server socket
            (= connection to client) */
uint32_t uo_server_getsockname(const UO::Server *server);
/** @return port, in network byte order, of our uo server socket
            (= connection to client) */
uint16_t uo_server_getsockport(const UO::Server *server);

/* utilities */

void uo_server_speak_ascii(UO::Server *server,
                           uint32_t serial,
                           int16_t graphic,
                           uint8_t type,
                           uint16_t hue, uint16_t font,
                           const char *name,
                           const char *text);

void uo_server_speak_console(UO::Server *server,
                             const char *text);
