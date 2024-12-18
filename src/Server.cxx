// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Server.hxx"
#include "SocketBuffer.hxx"
#include "Compression.hxx"
#include "PacketLengths.hxx"
#include "PacketStructs.hxx"
#include "Log.hxx"
#include "Encryption.hxx"
#include "event/DeferEvent.hxx"
#include "net/UniqueSocketDescriptor.hxx"

#include <utility>

#include <assert.h>
#include <stdlib.h>

namespace UO {

class Server final : SocketBufferHandler  {
public:
    SocketBuffer *const sock;
    uint32_t seed = 0;
    bool compression_enabled = false;

    struct encryption *encryption = encryption_new();

    enum protocol_version protocol_version = PROTOCOL_UNKNOWN;

    ServerHandler &handler;

    DeferEvent abort_event;

    explicit Server(EventLoop &event_loop,
                    UniqueSocketDescriptor &&s, ServerHandler &_handler) noexcept
        :sock(sock_buff_create(event_loop, std::move(s), 8192, 65536, *this)),
         handler(_handler),
         abort_event(event_loop, BIND_THIS_METHOD(DeferredAbort))
    {
    }

    ~Server() noexcept {
        encryption_free(encryption);

        sock_buff_dispose(sock);
    }

    void Abort() noexcept;

private:
    void DeferredAbort() noexcept;

    ssize_t ParsePackets(const uint8_t *data, size_t length);

    /* virtual methods from SocketBufferHandler */
    size_t OnSocketData(const void *data, size_t length) override;
    void OnSocketDisconnect(int error) noexcept override;
};

inline void
Server::DeferredAbort() noexcept
{
    handler.OnServerDisconnect();
}

} // namespace UO

void
UO::Server::Abort() noexcept
{
    /* this is a trick to delay the destruction of this object until
       everything is done */

    abort_event.Schedule();
}

inline ssize_t
UO::Server::ParsePackets(const uint8_t *data, size_t length)
{
    size_t consumed = 0;

    while (length > 0) {
        size_t packet_length = get_packet_length(protocol_version,
                                                 data, length);
        if (packet_length == PACKET_LENGTH_INVALID) {
            Log(1, "malformed packet from client\n");
            log_hexdump(5, data, length);
            Abort();
            return 0;
        }

        LogFmt(9, "from client: {:#02x} length={}\n",
            data[0], packet_length);

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
            Log(2, "zero seed from client\n");
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
            Log(2, "zero seed from client\n");
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
        Log(2, "client closed the connection\n");
    else
        log_error("error during communication with client", error);

    handler.OnServerDisconnect();
}

UO::Server *
uo_server_create(EventLoop &event_loop, UniqueSocketDescriptor &&s,
                 UO::ServerHandler &handler)
{
    s.SetNoDelay();

    return new UO::Server(event_loop, std::move(s), handler);
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
    assert(server->sock != nullptr || server->abort_event.IsPending());
    assert(length > 0);
    assert(get_packet_length(server->protocol_version, src, length) == length);

    if (server->abort_event.IsPending())
        return;

    LogFmt(9, "sending packet to client, length={}\n", length);
    log_hexdump(10, src, length);

    if (server->compression_enabled) {
        auto w = sock_buff_write(server->sock);
        if (w.empty()) {
            Log(1, "output buffer full in uo_server_send()\n");
            server->Abort();
            return;
        }

        ssize_t nbytes = uo_compress((unsigned char *)w.data(), w.size(),
                                     {(const unsigned char *)src, length});
        if (nbytes < 0) {
            Log(1, "uo_compress() failed\n");
            server->Abort();
            return;
        }

        sock_buff_append(server->sock, (size_t)nbytes);
    } else {
        if (!sock_buff_send(server->sock, src, length)) {
            Log(1, "output buffer full in uo_server_send()\n");
            server->Abort();
        }
    }
}
