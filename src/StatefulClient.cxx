// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "StatefulClient.hxx"
#include "Client.hxx"
#include "CVersion.hxx"
#include "Log.hxx"
#include "net/UniqueSocketDescriptor.hxx"

#include <assert.h>

StatefulClient::StatefulClient(EventLoop &event_loop) noexcept
	:ping_timer(event_loop, BIND_THIS_METHOD(OnPingTimer))
{
}

StatefulClient::~StatefulClient() noexcept = default;

inline const struct uo_packet_seed *
StatefulClient::GetSeedPacket(uint32_t seed, bool for_game_login,
			      struct uo_packet_seed &buffer) const noexcept
{
	const struct uo_packet_seed *seed_packet = for_game_login
		? nullptr
		: version.seed.get();

	if (!for_game_login && seed_packet == nullptr &&
	    version.IsDefined() &&
	    version.protocol >= ProtocolVersion::V6_0_14) {
		buffer.cmd = UO::Command::Seed;
		buffer.seed = seed;

		if (version.protocol >= ProtocolVersion::V7) {
			buffer.client_major = 7;
			buffer.client_minor = 0;
			buffer.client_revision = 10;
			buffer.client_patch = 3;
		} else {
			buffer.client_major = 6;
			buffer.client_minor = 0;
			buffer.client_revision = 14;
			buffer.client_patch = 2;
		}

		seed_packet = &buffer;
	}

	return seed_packet;
}

void
StatefulClient::Connect(EventLoop &event_loop, UniqueSocketDescriptor &&s,
			uint32_t seed, bool for_game_login,
			UO::PacketHandler &handler)
{
	assert(client == nullptr);

	struct uo_packet_seed seed_buffer;
	const struct uo_packet_seed *seed_packet = GetSeedPacket(seed, for_game_login,
								 seed_buffer);

	s.SetNoDelay();

	client.reset(new UO::Client(event_loop, std::move(s), seed,
				    seed_packet,
				    handler));

	client->SetProtocol(version.protocol);

	SchedulePing();

}

void
StatefulClient::Disconnect() noexcept
{
	assert(client != nullptr);

	version_requested = false;

	ping_timer.Cancel();

	client.reset();
}

inline void
StatefulClient::OnPingTimer() noexcept
{
	assert(client != nullptr);

	struct uo_packet_ping ping;
	ping.cmd = UO::Command::Ping;
	ping.id = ++ping_request;

	Log(2, "sending ping\n");
	client->SendT(ping);

	/* schedule next ping */
	SchedulePing();
}
