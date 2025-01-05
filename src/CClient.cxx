// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Connection.hxx"
#include "LinkedServer.hxx"
#include "SocketConnect.hxx"
#include "ProxySocks.hxx"
#include "Server.hxx"
#include "Log.hxx"
#include "Instance.hxx"
#include "Config.hxx"
#include "net/SocketAddress.hxx"

#include <assert.h>

bool
Connection::OnClientDisconnect() noexcept
{
	assert(client.client != nullptr);

	if (autoreconnect && IsInGame()) {
		Log(2, "server disconnected, auto-reconnecting\n");
		connection_speak_console(this, "uoproxy was disconnected, auto-reconnecting...");
		ScheduleReconnect();
		return false;
	} else {
		Log(1, "server disconnected\n");
		Destroy();
		return false;
	}
}

void
Connection::Connect(SocketAddress server_address,
		    uint32_t seed, bool for_game_login)
{
	assert(client.client == nullptr);

	UniqueSocketDescriptor fd;

	if (!instance.config.socks4_address.empty()) {
		const auto &socks4_address = instance.config.socks4_address.GetBest();
		fd = socket_connect(socks4_address.GetFamily(), SOCK_STREAM, 0, socks4_address);

		socks_connect(fd, server_address);
	} else {
		fd = socket_connect(server_address.GetFamily(), SOCK_STREAM, 0, server_address);
	}

	client.Connect(GetEventLoop(), std::move(fd), seed, for_game_login, *this);
}
