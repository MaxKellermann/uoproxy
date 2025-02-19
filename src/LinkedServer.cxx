// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "LinkedServer.hxx"
#include "Server.hxx"
#include "Connection.hxx"
#include "Log.hxx"
#include "uo/MakePacket.hxx"

#include <fmt/format.h>

#include <cassert>

unsigned LinkedServer::id_counter;

LinkedServer::LinkedServer(EventLoop &event_loop, UniqueSocketDescriptor &&s)
	:server(new UO::Server(event_loop, std::move(s), *this)),
	zombie_timeout(event_loop, BIND_THIS_METHOD(ZombieTimeoutCallback)),
	id(++id_counter)
{
}

LinkedServer::~LinkedServer() noexcept = default;

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
LinkedServer::Send(std::span<const std::byte> src)
{
	assert(server);

	server->Send(src);
}

void
LinkedServer::SendDivert(ProtocolVersion new_protocol,
			 std::span<const std::byte> old_packet,
			 std::span<const std::byte> new_packet) noexcept
{
	Send(client_version.protocol >= new_protocol ? new_packet : old_packet);
}

void
LinkedServer::SpeakConsole(std::string_view text)
{
	Send(UO::MakeSpeakConsole(text));
}

void
LinkedServer::ZombieTimeoutCallback() noexcept
{
	assert(state == State::RELAY_SERVER);

	connection->RemoveCheckEmpty(*this);
}

bool
LinkedServer::OnDisconnect() noexcept
{
	assert(server != nullptr);
	server.reset();

	if (state == State::RELAY_SERVER) {
		LogF(2, "client disconnected, zombifying server connection for 5 seconds");

		zombie_timeout.Schedule(std::chrono::seconds{5});
		return false;
	}

	Connection *c = connection;
	assert(c != nullptr);
	c->RemoveCheckEmpty(*this);

	delete this;
	return false;
}
