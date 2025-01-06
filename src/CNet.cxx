// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Connection.hxx"
#include "LinkedServer.hxx"
#include "uo/MakePacket.hxx"

#include <cassert>

void
Connection::SpeakConsole(const char *msg)
{
	BroadcastToInGameClients(UO::MakeSpeakConsole(msg));
}

void
Connection::BroadcastToInGameClients(std::span<const std::byte> src) noexcept
{
	for (auto &ls : servers)
		if (ls.IsInGame())
			ls.Send(src);
}

void
Connection::BroadcastToInGameClientsExcept(std::span<const std::byte> src,
					   LinkedServer &except) noexcept
{
	for (auto &ls : servers)
		if (&ls != &except && ls.IsInGame())
			ls.Send(src);
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
			ls.SendDivert(new_protocol, old_packet, new_packet);
		}
	}
}
