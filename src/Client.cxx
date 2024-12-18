// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Client.hxx"
#include "SocketBuffer.hxx"
#include "Compression.hxx"
#include "PacketLengths.hxx"
#include "PacketStructs.hxx"
#include "Log.hxx"
#include "uo/Command.hxx"
#include "net/UniqueSocketDescriptor.hxx"
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

    explicit Client(UniqueSocketDescriptor &&s, ClientHandler &_handler) noexcept
        :sock(sock_buff_create(std::move(s), 8192, 65536, *this)),
         handler(_handler)
    {
        evtimer_set(&abort_event,
                    uo_client_abort_event_callback, this);
    }

    ~Client() noexcept {
        sock_buff_dispose(sock);

        evtimer_del(&abort_event);
    }

    void Abort() noexcept;

private:
    ssize_t Decompress(std::span<const uint8_t> src);
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
UO::Client::Decompress(std::span<const uint8_t> src)
{
    auto w = decompressed_buffer.Write();
    if (w.empty()) {
        Log(1, "decompression buffer full\n");
        Abort();
        return -1;
    }

    ssize_t nbytes = uo_decompress(&decompression,
                                   w.data(), w.size(),
                                   src);
    if (nbytes < 0) {
        Log(1, "decompression failed\n");
        Abort();
        return -1;
    }

    decompressed_buffer.Append((size_t)nbytes);

    return src.size();
}

ssize_t
UO::Client::ParsePackets(const uint8_t *data, size_t length)
{
    size_t consumed = 0, packet_length;

    while (length > 0) {
        packet_length = get_packet_length(protocol_version, data, length);
        if (packet_length == PACKET_LENGTH_INVALID) {
            Log(1, "malformed packet from server\n");
            log_hexdump(5, data, length);
            Abort();
            return 0;
        }

        LogFmt(9, "from server: {:#02x} length={}\n",
            data[0], packet_length);

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

        nbytes = Decompress({data, length});
        if (nbytes <= 0)
            return 0;
        consumed = (size_t)nbytes;

        auto r = decompressed_buffer.Read();
        if (r.empty())
            return consumed;

        nbytes = ParsePackets(r.data(), r.size());
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
        Log(2, "server closed the connection\n");
    else
        log_error("error during communication with server", error);

    handler.OnClientDisconnect();
}

UO::Client *
uo_client_create(UniqueSocketDescriptor &&s, uint32_t seed,
                 const struct uo_packet_seed *seed6,
                 UO::ClientHandler &handler)
{
    s.SetNoDelay();

    auto *client = new UO::Client(std::move(s), handler);

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

    LogFmt(9, "sending packet to server, length={}\n", length);
    log_hexdump(10, src, length);

    if (*(const UO::Command *)src == UO::Command::GameLogin)
        client->compression_enabled = true;

    if (!sock_buff_send(client->sock, src, length)) {
        Log(1, "output buffer full in uo_client_send()\n");
        client->Abort();
    }
}
