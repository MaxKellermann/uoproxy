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

#include "Server.hxx"
#include "SocketBuffer.hxx"
#include "Compression.hxx"
#include "PacketLengths.hxx"
#include "PacketStructs.hxx"
#include "Log.hxx"
#include "SocketUtil.hxx"
#include "Encryption.hxx"
#include "util/WritableBuffer.hxx"

#include <utility>

#include <assert.h>
#include <stdlib.h>

#include <event.h>

static void
uo_server_abort_event_callback(int fd, short event, void *ctx) noexcept;

namespace UO {

class Server final : SocketBufferHandler  {
public:
    SocketBuffer *const sock;
    uint32_t seed = 0;
    bool compression_enabled = false;

    struct encryption *encryption = encryption_new();

    enum protocol_version protocol_version = PROTOCOL_UNKNOWN;

    ServerHandler &handler;

    bool aborted = false;
    struct event abort_event;

    explicit Server(int fd, ServerHandler &_handler) noexcept
        :sock(sock_buff_create(fd, 8192, 65536, *this)),
         handler(_handler)
    {
        evtimer_set(&abort_event,
                    uo_server_abort_event_callback, this);
    }

    ~Server() noexcept {
        encryption_free(encryption);

        sock_buff_dispose(sock);

        evtimer_del(&abort_event);
    }

    void Abort() noexcept;

private:
    ssize_t ParsePackets(const uint8_t *data, size_t length);

    /* virtual methods from SocketBufferHandler */
    size_t OnSocketData(const void *data, size_t length) override;
    void OnSocketDisconnect(int error) noexcept override;
};

} // namespace UO

static void
uo_server_abort_event_callback(int, short, void *ctx) noexcept
{
    auto server = (UO::Server *)ctx;

    assert(server->aborted);

    server->handler.OnServerDisconnect();
}

void
UO::Server::Abort() noexcept
{
    if (aborted)
        return;

    static constexpr struct timeval tv{0, 0};

    /* this is a trick to delay the destruction of this object until
       everything is done */
    evtimer_add(&abort_event, &tv);

    aborted = true;
}

inline ssize_t
UO::Server::ParsePackets(const uint8_t *data, size_t length)
{
    size_t consumed = 0;

    while (length > 0) {
        size_t packet_length = get_packet_length(protocol_version,
                                                 data, length);
        if (packet_length == PACKET_LENGTH_INVALID) {
            LogFormat(1, "malformed packet from client\n");
            log_hexdump(5, data, length);
            Abort();
            return 0;
        }

        LogFormat(9, "from client: 0x%02x length=%u\n",
            data[0], (unsigned)packet_length);

        if (packet_length == 0 || packet_length > length)
            break;

        log_hexdump(10, data, packet_length);

        if (!handler.OnServerPacket(data, packet_length))
            return -1;

        consumed += packet_length;
        data += packet_length;
        length -= packet_length;
    }

    return (ssize_t)consumed;
}

size_t
UO::Server::OnSocketData(const void *data0, size_t length)
{
    data0 = encryption_from_client(encryption, data0, length);
    if (data0 == nullptr)
        /* need more data */
        return 0;

    const uint8_t *data = (const uint8_t *)data0;
    size_t consumed = 0;

    if (seed == 0 && data[0] == 0xef) {
        /* client 6.0.5.0 sends a "0xef" seed packet instead of the
           raw 32 bit seed */
        auto p = (const struct uo_packet_seed *)data0;

        if (length < sizeof(*p))
            return 0;

        seed = p->seed;
        if (seed == 0) {
            LogFormat(2, "zero seed from client\n");
            Abort();
            return 0;
        }
    }

    if (seed == 0) {
        /* the first packet from a client is the seed, 4 bytes without
           header */
        if (length < 4)
            return 0;

        seed = *(const uint32_t*)(data + consumed);
        if (seed == 0) {
            LogFormat(2, "zero seed from client\n");
            Abort();
            return 0;
        }

        consumed += sizeof(uint32_t);
    }

    ssize_t nbytes = ParsePackets(data + consumed, length - consumed);
    if (nbytes < 0)
        return 0;

    return consumed + (size_t)nbytes;
}

void
UO::Server::OnSocketDisconnect(int error) noexcept
{
    if (error == 0)
        LogFormat(2, "client closed the connection\n");
    else
        log_error("error during communication with client", error);

    handler.OnServerDisconnect();
}

UO::Server *
uo_server_create(int sockfd,
                 UO::ServerHandler &handler)
{
    socket_set_nodelay(sockfd, 1);

    return new UO::Server(sockfd, handler);
}

void uo_server_dispose(UO::Server *server) {
    delete server;
}

uint32_t uo_server_seed(const UO::Server *server) {
    return server->seed;
}

void uo_server_set_compression(UO::Server *server, bool comp) {
    server->compression_enabled = comp;
}

void
uo_server_set_protocol(UO::Server *server,
                       enum protocol_version protocol_version)
{
    assert(server->protocol_version == PROTOCOL_UNKNOWN);

    server->protocol_version = protocol_version;
}

uint32_t uo_server_getsockname(const UO::Server *server)
{
    return sock_buff_sockname(server->sock);
}

uint16_t uo_server_getsockport(const UO::Server *server)
{
    return sock_buff_port(server->sock);
}

void uo_server_send(UO::Server *server,
                    const void *src, size_t length) {
    assert(server->sock != nullptr || server->aborted);
    assert(length > 0);
    assert(get_packet_length(server->protocol_version, src, length) == length);

    if (server->aborted)
        return;

    LogFormat(9, "sending packet to client, length=%u\n", (unsigned)length);
    log_hexdump(10, src, length);

    if (server->compression_enabled) {
        auto w = WritableBuffer<uint8_t>::FromVoid(sock_buff_write(server->sock));
        if (w.empty()) {
            LogFormat(1, "output buffer full in uo_server_send()\n");
            server->Abort();
            return;
        }

        ssize_t nbytes = uo_compress(w.data, w.size,
                                     (const unsigned char *)src, length);
        if (nbytes < 0) {
            LogFormat(1, "uo_compress() failed\n");
            server->Abort();
            return;
        }

        sock_buff_append(server->sock, (size_t)nbytes);
    } else {
        if (!sock_buff_send(server->sock, src, length)) {
            LogFormat(1, "output buffer full in uo_server_send()\n");
            server->Abort();
        }
    }
}
