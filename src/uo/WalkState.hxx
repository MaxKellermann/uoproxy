// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "PacketStructs.hxx"

#include <array>
#include <cstdint>

struct LinkedServer;

struct WalkState {
	struct Item {
		/**
		 * The walk packet sent by the client.
		 */
		struct uo_packet_walk packet;

		/**
		 * The walk sequence number which was sent to the server.
		 */
		uint8_t seq;
	};

	LinkedServer *server = nullptr;
	std::array<Item, 4> queue;
	unsigned queue_size = 0;
	uint8_t seq_next = 0;

	void clear() noexcept {
		queue_size = 0;
		server = nullptr;
	}

	void pop_front() noexcept;

	[[gnu::pure]]
	Item *FindSequence(uint8_t seq) noexcept;

	void Remove(Item &item) noexcept;

	void OnServerRemoved(LinkedServer &ls) noexcept {
		if (server == &ls)
			server = nullptr;
	}
};
