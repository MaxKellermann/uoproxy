// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Handler.hxx"

PacketAction
handle_packet_from_server(const struct client_packet_binding *bindings,
			  Connection &c,
			  std::span<const std::byte> src)
{
	const auto cmd = static_cast<UO::Command>(src.front());

	for (; bindings->handler != nullptr; bindings++) {
		if (bindings->cmd == cmd)
			return bindings->handler(c, src);
	}

	return PacketAction::ACCEPT;
}

PacketAction
handle_packet_from_client(const struct server_packet_binding *bindings,
			  LinkedServer &ls,
			  std::span<const std::byte> src)
{
	const auto cmd = static_cast<UO::Command>(src.front());

	for (; bindings->handler != nullptr; bindings++) {
		if (bindings->cmd == cmd)
			return bindings->handler(ls, src);
	}

	return PacketAction::ACCEPT;
}
