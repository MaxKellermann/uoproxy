// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Connection.hxx"
#include "LinkedServer.hxx"
#include "PacketStructs.hxx"
#include "Server.hxx"
#include "Client.hxx"
#include "Log.hxx"
#include "uo/Command.hxx"
#include "util/SpanCast.hxx"

static void
walk_cancel(const World &world,
	    UO::Server &server,
	    const struct uo_packet_walk &old)
{
	const struct uo_packet_walk_cancel p = {
		.cmd = UO::Command::WalkCancel,
		.seq = old.seq,
		.x = world.packet_start.x,
		.y = world.packet_start.y,
		.direction = world.packet_start.direction,
		.z = (int8_t)world.packet_start.z,
	};

	server.SendT(p);
}

void
connection_walk_request(LinkedServer &ls,
			const struct uo_packet_walk &p)
{
	auto &connection = *ls.connection;
	auto &state = connection.walk;

	if (state.queue_size > 0 && &ls != state.server) {
		ls.LogF(2, "rejecting walk");
		walk_cancel(connection.client.world, *ls.server, p);
		return;
	}

	if (state.queue_size >= state.queue.size()) {
		/* XXX */
		ls.LogF(2, "walk queue full");
		walk_cancel(connection.client.world, *ls.server,
			    state.queue[0].packet);
		state.pop_front();
	}

	state.server = &ls;
	auto *i = &state.queue[state.queue_size++];
	i->packet = p;

	ls.LogF(7, "walk seq_from_client={} seq_to_server={}",
		p.seq, state.seq_next);

	auto walk = p;
	walk.seq = i->seq = (uint8_t)state.seq_next++;
	connection.client.client->SendT(walk);

	if (state.seq_next == 0)
		state.seq_next = 1;
}

inline void
Connection::Resynchronize()
{
	walk.clear();

	const struct uo_packet_walk_ack packet = {
		.cmd = UO::Command::Resynchronize,
		.seq = 0,
		.notoriety = 0,
	};

	client.client->SendT(packet);
}

void
Connection::OnWalkCancel(const struct uo_packet_walk_cancel &p)
{
	walk.seq_next = 0;

	auto *i = walk.FindSequence(p.seq);

	if (i != nullptr)
		LogFmt(7, "walk_cancel seq_to_client={} seq_from_server={}\n",
		       i->packet.seq, p.seq);
	else
		LogFmt(7, "walk_cancel seq_from_server={}\n", p.seq);

	client.world.WalkCancel(p.x, p.y, p.direction);

	/* only send to requesting client */

	if (i != nullptr && walk.server != nullptr) {
		auto cancel = p;
		cancel.seq = i->packet.seq;
		walk.server->server->SendT(cancel);
	}

	walk.clear();
}

void
Connection::OnWalkAck(const struct uo_packet_walk_ack &p)
{
	auto *i = walk.FindSequence(p.seq);
	if (i == nullptr) {
		Log(1, "WalkAck out of sync\n");
		Resynchronize();
		return;
	}

	LogFmt(7, "walk_ack seq_to_client={} seq_from_server={}\n",
	       i->packet.seq, p.seq);

	unsigned x = client.world.packet_start.x;
	unsigned y = client.world.packet_start.y;

	if ((client.world.packet_start.direction & 0x07) == (i->packet.direction & 0x07)) {
		switch (i->packet.direction & 0x07) {
		case 0: /* north */
			--y;
			break;
		case 1: /* north east */
			++x;
			--y;
			break;
		case 2: /* east */
			++x;
			break;
		case 3: /* south east */
			++x;
			++y;
			break;
		case 4: /* south */
			++y;
			break;
		case 5: /* south west */
			--x;
			++y;
			break;
		case 6: /* west */
			--x;
			break;
		case 7: /* north west */
			--x;
			--y;
			break;
		}
	}

	client.world.Walked(x, y, i->packet.direction, p.notoriety);

	/* forward ack to requesting client */
	if (walk.server != nullptr) {
		auto ack = p;
		ack.seq = i->packet.seq;
		walk.server->server->SendT(ack);
	}

	/* send WalkForce to all other clients */

#if 0
	/* unfortunately, this doesn't work anymore with client v7 */
	struct uo_packet_walk_force force = {
		.cmd = UO::Command::WalkForce,
		.direction = i->packet.direction & 0x7,
	};
#endif

	const struct uo_packet_mobile_update *mu =
		&client.world.packet_mobile_update;

	if (walk.server != nullptr)
		BroadcastToInGameClientsExcept(ReferenceAsBytes(mu),
					       *walk.server);
	else
		BroadcastToInGameClients(ReferenceAsBytes(mu));

	walk.Remove(*i);
}
