// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Connection.hxx"
#include "LinkedServer.hxx"
#include "SocketConnect.hxx"
#include "ProxySocks.hxx"
#include "Server.hxx"
#include "Handler.hxx"
#include "Log.hxx"
#include "Instance.hxx"
#include "Config.hxx"
#include "net/SocketAddress.hxx"

#include <assert.h>
#include <unistd.h>

bool
Connection::OnClientPacket(std::span<const std::byte> src)
{
	assert(client.client != nullptr);

	const auto action = handle_packet_from_server(server_packet_bindings,
						      *this, src);
	switch (action) {
	case PacketAction::ACCEPT:
		if (!client.reconnecting)
			BroadcastToInGameClients(src);

		break;

	case PacketAction::DROP:
		break;

	case PacketAction::DISCONNECT:
		LogFmt(2, "aborting connection to server after packet {:#02x}\n",
			  src.front());
		log_hexdump(6, src.data(), src.size());

		if (autoreconnect && IsInGame()) {
			Log(2, "auto-reconnecting\n");
			ScheduleReconnect();
		} else {
			Destroy();
		}
		return false;

	case PacketAction::DELETED:
		return false;
	}

	return true;
}

void
Connection::OnClientDisconnect() noexcept
{
	assert(client.client != nullptr);

	if (autoreconnect && IsInGame()) {
		Log(2, "server disconnected, auto-reconnecting\n");
		connection_speak_console(this, "uoproxy was disconnected, auto-reconnecting...");
		ScheduleReconnect();
	} else {
		Log(1, "server disconnected\n");
		Destroy();
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
