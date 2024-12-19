// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "LinkedServer.hxx"
#include "Connection.hxx"
#include "Handler.hxx"
#include "Log.hxx"

#include <fmt/format.h>

#include <cassert>

unsigned LinkedServer::id_counter;

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
LinkedServer::ZombieTimeoutCallback() noexcept
{
	assert(state == State::RELAY_SERVER);

	connection->RemoveCheckEmpty(*this);
}

bool
LinkedServer::OnServerPacket(std::span<const std::byte> src)
{
	Connection *c = connection;

	assert(c != nullptr);
	assert(server != nullptr);

	const auto action = handle_packet_from_client(client_packet_bindings,
						      *this, src);
	switch (action) {
	case PacketAction::ACCEPT:
		if (c->client.client != nullptr &&
		    (!c->client.reconnecting ||
		     static_cast<UO::Command>(src.front()) == UO::Command::ClientVersion))
			c->client.client->Send(src.data(), src.size());
		break;

	case PacketAction::DROP:
		break;

	case PacketAction::DISCONNECT:
		LogF(2, "aborting connection to client after packet {:#x}", src.front());
		log_hexdump(6, src.data(), src.size());

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
	server.reset();

	if (state == State::RELAY_SERVER) {
		LogF(2, "client disconnected, zombifying server connection for 5 seconds");

		zombie_timeout.Schedule(std::chrono::seconds{5});
		return;
	}

	Connection *c = connection;
	assert(c != nullptr);
	c->RemoveCheckEmpty(*this);

	delete this;
}
