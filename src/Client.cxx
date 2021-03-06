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

#include "Client.hxx"
#include "SocketBuffer.hxx"
#include "Compression.hxx"
#include "PacketLengths.hxx"
#include "PacketStructs.hxx"
#include "PacketType.hxx"
#include "Log.hxx"
#include "SocketUtil.hxx"
#include "util/DynamicFifoBuffer.hxx"

#include <utility>

#include <assert.h>
#include <stdlib.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#endif

#include <event.h>

static void
uo_client_abort_event_callback(int fd,
                               short event,
                               void *ctx) noexcept;

namespace UO {

class Client final : SocketBufferHandler {
public:
    SocketBuffer *const sock;
    bool compression_enabled = false;
    struct uo_decompression decompression;
    DynamicFifoBuffer<uint8_t> decompressed_buffer{65536};

    enum protocol_version protocol_version = PROTOCOL_UNKNOWN;

    ClientHandler &handler;

    bool aborted = false;
    struct event abort_event;

    explicit Client(int fd, ClientHandler &_handler) noexcept
        :sock(sock_buff_create(fd, 8192, 65536, *this)),
         handler(_handler)
    {
        uo_decompression_init(&decompression);

        evtimer_set(&abort_event,
                    uo_client_abort_event_callback, this);
    }

    ~Client() noexcept {
        sock_buff_dispose(sock);

        evtimer_del(&abort_event);
    }

    void Abort() noexcept;

private:
    ssize_t Decompress(const uint8_t *data, size_t length);
    ssize_t ParsePackets(const uint8_t *data, size_t length);

    /* virtual methods from SocketBufferHandler */
    size_t OnSocketData(const void *data, size_t length) override;
    void OnSocketDisconnect(int error) noexcept override;
};

} // namespace UO

static void
uo_client_abort_event_callback(int, short, void *ctx) noexcept
{
    auto client = (UO::Client *)ctx;

    assert(client->aborted);

    client->handler.OnClientDisconnect();
}

void
UO::Client::Abort() noexcept
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
UO::Client::Decompress(const uint8_t *data, size_t length)
{
    auto w = decompressed_buffer.Write();
    if (w.empty()) {
        LogFormat(1, "decompression buffer full\n");
        Abort();
        return -1;
    }

    ssize_t nbytes = uo_decompress(&decompression,
                                   w.data, w.size,
                                   data, length);
    if (nbytes < 0) {
        LogFormat(1, "decompression failed\n");
        Abort();
        return -1;
    }

    decompressed_buffer.Append((size_t)nbytes);

    return (size_t)length;
}

ssize_t
UO::Client::ParsePackets(const uint8_t *data, size_t length)
{
    size_t consumed = 0, packet_length;

    while (length > 0) {
        packet_length = get_packet_length(protocol_version, data, length);
        if (packet_length == PACKET_LENGTH_INVALID) {
            LogFormat(1, "malformed packet from server\n");
            log_hexdump(5, data, length);
            Abort();
            return 0;
        }

        LogFormat(9, "from server: 0x%02x length=%u\n",
            data[0], (unsigned)packet_length);

        if (packet_length == 0 || packet_length > length)
            break;

        log_hexdump(10, data, packet_length);

        if (!handler.OnClientPacket(data, packet_length))
            return -1;

        consumed += packet_length;
        data += packet_length;
        length -= packet_length;
    }

    return (ssize_t)consumed;
}

size_t
UO::Client::OnSocketData(const void *data0, size_t length)
{
    const uint8_t *data = (const uint8_t *)data0;

    if (compression_enabled) {
        ssize_t nbytes;
        size_t consumed;

        nbytes = Decompress(data, length);
        if (nbytes <= 0)
            return 0;
        consumed = (size_t)nbytes;

        auto r = decompressed_buffer.Read();
        if (r.empty())
            return consumed;

        nbytes = ParsePackets(r.data, r.size);
        if (nbytes < 0)
            return 0;

        decompressed_buffer.Consume((size_t)nbytes);

        return consumed;
    } else {
        ssize_t nbytes = ParsePackets(data, length);
        if (nbytes < 0)
            return 0;

        return nbytes;
    }
}

void
UO::Client::OnSocketDisconnect(int error) noexcept
{
    if (error == 0)
        LogFormat(2, "server closed the connection\n");
    else
        log_error("error during communication with server", error);

    handler.OnClientDisconnect();
}

UO::Client *
uo_client_create(int fd, uint32_t seed,
                 const struct uo_packet_seed *seed6,
                 UO::ClientHandler &handler)
{
    socket_set_nodelay(fd, 1);

    auto *client = new UO::Client(fd, handler);

    /* seed must be the first 4 bytes, and it must be flushed */
    if (seed6 != nullptr) {
        struct uo_packet_seed p = *seed6;
        p.seed = seed;
        uo_client_send(client, &p, sizeof(p));
    } else {
        PackedBE32 seed_be(seed);
        uo_client_send(client, &seed_be, sizeof(seed_be));
    }

    return client;
}

void uo_client_dispose(UO::Client *client) {
    delete client;
}

void
uo_client_set_protocol(UO::Client *client,
                       enum protocol_version protocol_version)
{
    assert(client->protocol_version == PROTOCOL_UNKNOWN);

    client->protocol_version = protocol_version;
}

void uo_client_send(UO::Client *client,
                    const void *src, size_t length) {
    assert(client->sock != nullptr || client->aborted);
    assert(length > 0);

    if (client->aborted)
        return;

    LogFormat(9, "sending packet to server, length=%u\n", (unsigned)length);
    log_hexdump(10, src, length);

    if (*(const uint8_t*)src == PCK_GameLogin)
        client->compression_enabled = true;

    if (!sock_buff_send(client->sock, src, length)) {
        LogFormat(1, "output buffer full in uo_client_send()\n");
        client->Abort();
    }
}
