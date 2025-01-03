// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Connection.hxx"
#include "Instance.hxx"
#include "Client.hxx"
#include "Config.hxx"
#include "Log.hxx"
#include "net/SocketAddress.hxx"

#include <assert.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#endif

void
Connection::Disconnect() noexcept
{
	if (client.client == nullptr)
		return;

	client.reconnecting = false;
	reconnect_timer.Cancel();

	client.Disconnect();
	ClearWorld();
}

void
Connection::DoReconnect() noexcept
{
	const auto &config = instance.config;
	uint32_t seed;

	assert(IsInGame());
	assert(client.reconnecting);
	assert(client.client == nullptr);

	if (client.version.seed != nullptr)
		seed = client.version.seed->seed;
	else
		seed = 0xc0a80102; /* 192.168.1.2 */

	if (config.login_address.empty()) {
		/* connect to game server */
		assert(server_index < config.game_servers.size());
		const auto &server_address
			= config.game_servers[server_index].address;

		try {
			Connect(server_address, seed, true);
		} catch (...) {
			log_error("reconnect failed", std::current_exception());
			ScheduleReconnect();
			return;
		}

		const struct uo_packet_game_login p = {
			.cmd = UO::Command::GameLogin,
			.auth_id = seed,
			.credentials = credentials,
		};

		Log(2, "connected, doing GameLogin\n");

		client.client->SendT(p);
	} else {
		/* connect to login server */

		try {
			Connect(config.login_address.GetBest(), seed, false);
		} catch (...) {
			log_error("reconnect failed", std::current_exception());
			ScheduleReconnect();
			return;
		}

		const struct uo_packet_account_login p = {
			.cmd = UO::Command::AccountLogin,
			.credentials = credentials,
			.unknown1 = {},
		};

		Log(2, "connected, doing AccountLogin\n");

		client.client->SendT(p);
	}
}

void
Connection::ReconnectTimerCallback() noexcept
{
	DoReconnect();
}

void
Connection::Reconnect()
{
	Disconnect();

	assert(IsInGame());
	assert(client.client == nullptr);

	client.reconnecting = true;

	DoReconnect();
}

void
Connection::ScheduleReconnect() noexcept
{
	Disconnect();

	assert(IsInGame());
	assert(client.client == nullptr);

	client.reconnecting = true;

	reconnect_timer.Schedule(std::chrono::seconds{5});
}
