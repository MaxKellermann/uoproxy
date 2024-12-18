// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "LinkedServer.hxx"
#include "Connection.hxx"
#include "Handler.hxx"
#include "Log.hxx"

#include <fmt/format.h>

#include <cassert>

unsigned LinkedServer::id_counter;

LinkedServer::~LinkedServer() noexcept
{
    evtimer_del(&zombie_timeout);

    if (server != nullptr)
        uo_server_dispose(server);
}

void
LinkedServer::LogVFmt(unsigned level, fmt::string_view format_str, fmt::format_args args) noexcept
{
    if (level > verbose)
        return;

    fmt::memory_buffer buffer;
    fmt::vformat_to(std::back_inserter(buffer), format_str, args);

    LogFmt(level, "[client {}] {}\n", id, fmt::string_view{buffer.data(), buffer.size()});
}

void
LinkedServer::ZombieTimeoutCallback(int, short, void *ctx) noexcept
{
    auto &ls = *(LinkedServer *)ctx;
    assert(ls.state == LinkedServer::State::RELAY_SERVER);

    auto &c = *ls.connection;
    c.RemoveCheckEmpty(ls);
}

bool
LinkedServer::OnServerPacket(const void *data, size_t length)
{
    Connection *c = connection;

    assert(c != nullptr);
    assert(server != nullptr);

    const auto action = handle_packet_from_client(client_packet_bindings,
                                                  *this, data, length);
    switch (action) {
    case PacketAction::ACCEPT:
        if (c->client.client != nullptr &&
            (!c->client.reconnecting ||
             *(const UO::Command *)data == UO::Command::ClientVersion))
            uo_client_send(c->client.client, data, length);
        break;

    case PacketAction::DROP:
        break;

    case PacketAction::DISCONNECT:
        LogF(2, "aborting connection to client after packet {:#x}",
                  *(const unsigned char*)data);
        log_hexdump(6, data, length);

        c->Remove(*this);

        if (c->servers.empty()) {
            if (c->background)
                LogF(1, "backgrounding");
            else
                c->Destroy();
        }

        delete this;
        return false;

    case PacketAction::DELETED:
        return false;
    }

    return true;
}

void
LinkedServer::OnServerDisconnect() noexcept
{
    assert(server != nullptr);
    uo_server_dispose(std::exchange(server, nullptr));

    if (state == State::RELAY_SERVER) {
        LogF(2, "client disconnected, zombifying server connection for 5 seconds");

        static constexpr struct timeval tv{5, 0};
        evtimer_add(&zombie_timeout, &tv);
        return;
    }

    Connection *c = connection;
    assert(c != nullptr);
    c->RemoveCheckEmpty(*this);

    delete this;
}
