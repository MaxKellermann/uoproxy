// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <cstddef>
#include <span>

class UniqueSocketDescriptor;
class EventLoop;

namespace UO {

/**
 * A handler class for #Client and #Server.
 */
class PacketHandler {
public:
	/**
	 * A packet has been received.
	 *
	 * @return false if this object has been closed within the
	 * function
	 */
	virtual bool OnPacket(std::span<const std::byte> src) = 0;

	/**
	 * The connection has been closed due to an error or because the
	 * peer closed his side.
	 */
	virtual bool OnDisconnect() noexcept = 0;
};

} // namespace UO
