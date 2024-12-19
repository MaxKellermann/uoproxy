// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Connection.hxx"
#include "LinkedServer.hxx"
#include "PacketStructs.hxx"
#include "Log.hxx"

#include <assert.h>

void
Connection::Add(LinkedServer &ls) noexcept
{
	assert(ls.connection == nullptr);

	servers.push_front(ls);
	ls.connection = this;
}

void
Connection::Remove(LinkedServer &ls) noexcept
{
	assert(ls.connection == this);

	connection_walk_server_removed(walk, ls);

	ls.connection = nullptr;
	ls.unlink();
}

void
Connection::RemoveCheckEmpty(LinkedServer &ls) noexcept
{
	Remove(ls);

	if (!servers.empty()) {
		ls.LogF(2, "client disconnected, server connection still in use");
	} else if (background && client.IsConnected() && client.IsInGame()) {
		ls.LogF(1, "client disconnected, backgrounding");
	} else {
		ls.LogF(1, "last client disconnected, removing connection");
		Destroy();
	}
}

LinkedServer *
Connection::FindZombie(const struct uo_packet_game_login &game_login) noexcept
{
	if (game_login.credentials != credentials)
		return nullptr;

	for (auto &i : servers) {
		if (i.IsZombie() && i.auth_id == game_login.auth_id)
			return &i;
	}

	return nullptr;
}
