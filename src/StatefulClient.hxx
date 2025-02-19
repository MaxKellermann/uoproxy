// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "CVersion.hxx"
#include "World.hxx"
#include "event/CoarseTimerEvent.hxx"

#include <memory>

class EventLoop;
class UniqueSocketDescriptor;

namespace UO {
class Client;
class PacketHandler;
}

struct StatefulClient {
	bool reconnecting = false, version_requested = false;

	std::unique_ptr<UO::Client> client;
	CoarseTimerEvent ping_timer;

	ClientVersion version;

	/**
	 * The most recent game server list packet received from the
	 * server.  This does not get cleared when reconnecting, but gets
	 * overwritten when a new one is received.
	 */
	VarStructPtr<struct uo_packet_server_list> server_list;

	/**
	 * The most recent character list packet received from the server.
	 * This does not get cleared when reconnecting, but gets
	 * overwritten when a new one is received.
	 */
	VarStructPtr<struct uo_packet_simple_character_list> char_list;

	uint32_t supported_features_flags = 0;

	uint8_t ping_request = 0, ping_ack = 0;

	World world;

	explicit StatefulClient(EventLoop &event_loop) noexcept;
	~StatefulClient() noexcept;

	StatefulClient(const StatefulClient &) = delete;
	StatefulClient &operator=(const StatefulClient &) = delete;

	bool IsInGame() const noexcept {
		return world.HasStart();
	}

	bool IsConnected() const noexcept {
		return client != nullptr;
	}

	void Connect(EventLoop &event_loop, UniqueSocketDescriptor &&s,
		     uint32_t seed, bool for_game_login,
		     UO::PacketHandler &handler);

	void Disconnect() noexcept;

	void SchedulePing() noexcept {
		ping_timer.Schedule(std::chrono::seconds{30});
	}

private:
	[[nodiscard]]
	const struct uo_packet_seed *GetSeedPacket(uint32_t seed, bool for_game_login,
						   struct uo_packet_seed &buffer) const noexcept;

	void OnPingTimer() noexcept;
};
