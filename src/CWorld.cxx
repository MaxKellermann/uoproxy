// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Connection.hxx"
#include "LinkedServer.hxx"
#include "Server.hxx"
#include "util/SpanCast.hxx"

void
Connection::DeleteItems() noexcept
{
	client.world.items.clear_and_dispose([this](Item *i){
		const struct uo_packet_delete p{
			.cmd = UO::Command::Delete,
			.serial = i->serial,
		};

		BroadcastToInGameClients(ReferenceAsBytes(p));

		delete i;
	});
}

void
Connection::DeleteMobiles() noexcept
{
	client.world.mobiles.clear_and_dispose([this](Mobile *m){
		const struct uo_packet_delete p{
			.cmd = UO::Command::Delete,
			.serial = m->serial,
		};

		BroadcastToInGameClients(ReferenceAsBytes(p));

		delete m;
	});
}
