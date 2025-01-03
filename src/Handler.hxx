// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace UO { enum class Command : uint8_t; }

struct Connection;
struct LinkedServer;

/** what to do with the packet? */
enum class PacketAction {
	/** forward the packet to the other communication partner */
	ACCEPT,

	/** drop the packet */
	DROP,

	/** disconnect the endpoint from which this packet was received */
	DISCONNECT,

	/** the endpoint has been deleted */
	DELETED,
};

struct client_packet_binding {
	UO::Command cmd;
	PacketAction (*handler)(Connection &c,
				std::span<const std::byte> src);
};

struct server_packet_binding {
	UO::Command cmd;
	PacketAction (*handler)(LinkedServer &ls,
				std::span<const std::byte> src);
};

extern const struct client_packet_binding server_packet_bindings[];
extern const struct server_packet_binding client_packet_bindings[];

PacketAction
handle_packet_from_server(const struct client_packet_binding *bindings,
			  Connection &c,
			  std::span<const std::byte> src);

PacketAction
handle_packet_from_client(const struct server_packet_binding *bindings,
			  LinkedServer &ls,
			  std::span<const std::byte> src);
