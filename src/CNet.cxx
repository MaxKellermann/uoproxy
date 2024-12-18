// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Connection.hxx"
#include "LinkedServer.hxx"
#include "Server.hxx"

#include <assert.h>

/** broadcast a message to all clients */
void
connection_speak_console(Connection *c, const char *msg)
{
	for (auto &ls : c->servers) {
		if (ls.IsInGame()) {
			assert(ls.server != nullptr);

			ls.server->SpeakConsole(msg);
		}
	}
}

void
Connection::BroadcastToInGameClients(const void *data, size_t length) noexcept
{
	for (auto &ls : servers)
		if (ls.IsInGame())
			ls.server->Send(data, length);
}

void
Connection::BroadcastToInGameClientsExcept(const void *data, size_t length,
					   LinkedServer &except) noexcept
{
	for (auto &ls : servers)
		if (&ls != &except && ls.IsInGame())
			ls.server->Send(data, length);
}

void
Connection::BroadcastToInGameClientsDivert(enum protocol_version new_protocol,
					   const void *old_data, size_t old_length,
					   const void *new_data, size_t new_length) noexcept
{
	assert(new_protocol > PROTOCOL_UNKNOWN);
	assert(old_data != nullptr);
	assert(old_length > 0);
	assert(new_data != nullptr);
	assert(new_length > 0);

	for (auto &ls : servers) {
		if (ls.IsInGame()) {
			if (ls.client_version.protocol >= new_protocol)
				ls.server->Send(new_data, new_length);
			else
				ls.server->Send(old_data, old_length);
		}
	}
}
