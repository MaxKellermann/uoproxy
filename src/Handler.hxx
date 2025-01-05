// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace UO { enum class Command : uint8_t; }

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

template<typename T, typename B>
PacketAction
DispatchPacket(T &t, const B *bindings, std::span<const std::byte> packet)
{
	const auto cmd = static_cast<UO::Command>(packet.front());

	for (; bindings->method != nullptr; bindings++) {
		if (bindings->cmd == cmd)
			return (t.*bindings->method)(packet);
	}

	return PacketAction::ACCEPT;
}
