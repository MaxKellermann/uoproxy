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

void
StatefulClient::Connect(EventLoop &event_loop, UniqueSocketDescriptor &&s,
			uint32_t seed, bool for_game_login,
			UO::ClientHandler &handler)
{
	assert(client == nullptr);

	const struct uo_packet_seed *seed_packet = for_game_login
		? nullptr
		: version.seed;
	struct uo_packet_seed seed_buffer;

	if (!for_game_login && seed_packet == nullptr &&
	    version.IsDefined() &&
	    version.protocol >= PROTOCOL_6_0_14) {
		seed_buffer.cmd = UO::Command::Seed;
		seed_buffer.seed = seed;

		if (version.protocol >= PROTOCOL_7) {
			seed_buffer.client_major = 7;
			seed_buffer.client_minor = 0;
			seed_buffer.client_revision = 10;
			seed_buffer.client_patch = 3;
		} else {
			seed_buffer.client_major = 6;
			seed_buffer.client_minor = 0;
			seed_buffer.client_revision = 14;
			seed_buffer.client_patch = 2;
		}

		seed_packet = &seed_buffer;
	}

	s.SetNoDelay();

	client = uo_client_create(event_loop, std::move(s), seed,
				  seed_packet,
				  handler);

	uo_client_set_protocol(client, version.protocol);

	SchedulePing();

}

void
StatefulClient::Disconnect() noexcept
{
	assert(client != nullptr);

	version_requested = false;

	ping_timer.Cancel();

	uo_client_dispose(client);
	client = nullptr;
}

inline void
StatefulClient::OnPingTimer() noexcept
{
	assert(client != nullptr);

	struct uo_packet_ping ping;
	ping.cmd = UO::Command::Ping;
	ping.id = ++ping_request;

	Log(2, "sending ping\n");
	uo_client_send(client, &ping, sizeof(ping));

	/* schedule next ping */
	SchedulePing();
}
