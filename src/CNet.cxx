// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Connection.hxx"
#include "LinkedServer.hxx"
#include "Server.hxx"

#include <assert.h>

/** broadcast a message to all clients */
void
connection_speak_console(Connection *c, const char *msg)
{
	for (auto &ls : c->servers) {
		if (ls.IsInGame()) {
			assert(ls.server != nullptr);

			ls.server->SpeakConsole(msg);
		}
	}
}

void
Connection::BroadcastToInGameClients(std::span<const std::byte> src) noexcept
{
	for (auto &ls : servers)
		if (ls.IsInGame())
			ls.server->Send(src);
}

void
Connection::BroadcastToInGameClientsExcept(std::span<const std::byte> src,
					   LinkedServer &except) noexcept
{
	for (auto &ls : servers)
		if (&ls != &except && ls.IsInGame())
			ls.server->Send(src);
}

void
Connection::BroadcastToInGameClientsDivert(ProtocolVersion new_protocol,
					   std::span<const std::byte> old_packet,
					   std::span<const std::byte> new_packet) noexcept
{
	assert(new_protocol > ProtocolVersion::UNKNOWN);
	assert(!old_packet.empty());
	assert(!new_packet.empty());

	for (auto &ls : servers) {
		if (ls.IsInGame()) {
			if (ls.client_version.protocol >= new_protocol)
				ls.server->Send(new_packet);
			else
				ls.server->Send(old_packet);
		}
	}
}
