// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Handler.hxx"

PacketAction
handle_packet_from_server(const struct client_packet_binding *bindings,
			  Connection &c,
			  const void *data, size_t length)
{
	const unsigned char cmd
		= *(const unsigned char*)data;

	for (; bindings->handler != nullptr; bindings++) {
		if (bindings->cmd == cmd)
			return bindings->handler(c, data, length);
	}

	return PacketAction::ACCEPT;
}

PacketAction
handle_packet_from_client(const struct server_packet_binding *bindings,
			  LinkedServer &ls,
			  const void *data, size_t length)
{
	const unsigned char cmd
		= *(const unsigned char*)data;

	for (; bindings->handler != nullptr; bindings++) {
		if (bindings->cmd == cmd)
			return bindings->handler(ls, data, length);
	}

	return PacketAction::ACCEPT;
}
