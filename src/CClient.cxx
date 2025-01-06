// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Connection.hxx"
#include "LinkedServer.hxx"
#include "ProxySocks.hxx"
#include "Client.hxx"
#include "Log.hxx"
#include "Instance.hxx"
#include "Config.hxx"
#include "event/net/CoConnectSocket.hxx"
#include "net/SocketAddress.hxx"
#include "co/Task.hxx"
#include "util/SpanCast.hxx"

#include <cassert>

bool
Connection::OnDisconnect() noexcept
{
	assert(client.client != nullptr);

	if (autoreconnect && IsInGame()) {
		Log(2, "server disconnected, auto-reconnecting\n");
		SpeakConsole("uoproxy was disconnected, auto-reconnecting...");
		ScheduleReconnect();
		return false;
	} else {
		Log(1, "server disconnected\n");
		Destroy();
		return false;
	}
}

Co::InvokeTask
Connection::CoConnect(SocketAddress server_address,
		      uint32_t seed, bool for_game_login)
{
	assert(client.client == nullptr);

	UniqueSocketDescriptor fd;

	if (!instance.config.socks4_address.empty()) {
		const auto &socks4_address = instance.config.socks4_address.GetBest();
		fd = co_await CoConnectSocket(GetEventLoop(), socks4_address, std::chrono::seconds{30});

		co_await ProxySocksConnect(GetEventLoop(), fd, server_address);
	} else {
		fd = co_await CoConnectSocket(GetEventLoop(), server_address, std::chrono::seconds{30});
	}

	client.Connect(GetEventLoop(), std::move(fd), seed, for_game_login, *this);

	if (for_game_login) {
		const struct uo_packet_game_login login{
			.cmd = UO::Command::GameLogin,
			.auth_id = seed,
			.credentials = credentials,
		};

		Log(2, "connected, doing GameLogin\n");

		client.client->SendT(login);
	} else {
		const struct uo_packet_account_login p = {
			.cmd = UO::Command::AccountLogin,
			.credentials = credentials,
		};

		Log(2, "connected, doing AccountLogin\n");

		client.client->SendT(p);
	}
}

inline void
Connection::OnConnectComplete(std::exception_ptr error) noexcept
{
	if (error) {
		log_error("connect failed", std::move(error));

		if (client.reconnecting)
			ScheduleReconnect();
		else {
			static constexpr struct uo_packet_account_login_reject response{
				.cmd = UO::Command::AccountLoginReject,
				.reason = 0x02, /* blocked */
			};

			for (auto &ls : servers)
				if (ls.IsAtAccountLogin())
					ls.Send(ReferenceAsBytes(response));
		}
	}
}

void
Connection::ConnectAsync(SocketAddress server_address,
			 uint32_t seed, bool for_game_login) noexcept
{
	assert(client.client == nullptr);

	connect_task = CoConnect(server_address, seed, for_game_login);
	connect_task.Start(BIND_THIS_METHOD(OnConnectComplete));
}
