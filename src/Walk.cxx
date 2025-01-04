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

#include <algorithm>

#include <assert.h>

static void
walk_shift(WalkState &state)
{
	assert(state.queue_size > 0);

	std::move(state.queue.data() + 1, state.queue.data() + state.queue_size,
		  state.queue.data());

	--state.queue_size;

	if (state.queue_size == 0)
		state.server = nullptr;
}

static WalkState::Item *
find_by_seq(WalkState &state,
	    uint8_t seq)
{
	unsigned i;

	for (i = 0; i < state.queue_size; i++)
		if (state.queue[i].seq == seq)
			return &state.queue[i];

	return nullptr;
}

static void
remove_item(WalkState &state,
	    WalkState::Item *item)
{
	assert(item >= state.queue.data());
	assert(item < state.queue.data() + state.queue_size);

	std::move(item + 1, state.queue.data() + state.queue_size, item);

	--state.queue_size;

	if (state.queue_size == 0)
		state.server = nullptr;
}

static void
walk_clear(WalkState &state)
{
	state.server = nullptr;
	state.queue_size = 0;
}

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
connection_walk_server_removed(WalkState &state,
			       LinkedServer &ls) noexcept
{
	if (state.server != &ls)
		return;

	state.server = nullptr;
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
		walk_shift(state);
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

static void
connection_resync(Connection &c)
{
	walk_clear(c.walk);

	const struct uo_packet_walk_ack packet = {
		.cmd = UO::Command::Resynchronize,
		.seq = 0,
		.notoriety = 0,
	};

	c.client.client->SendT(packet);
}

void
connection_walk_cancel(Connection &c,
		       const struct uo_packet_walk_cancel &p)
{
	auto &state = c.walk;

	state.seq_next = 0;

	auto *i = find_by_seq(state, p.seq);

	if (i != nullptr)
		LogFmt(7, "walk_cancel seq_to_client={} seq_from_server={}\n",
		       i->packet.seq, p.seq);
	else
		LogFmt(7, "walk_cancel seq_from_server={}\n", p.seq);

	c.client.world.WalkCancel(p.x, p.y, p.direction);

	/* only send to requesting client */

	if (i != nullptr && state.server != nullptr) {
		auto cancel = p;
		cancel.seq = i->packet.seq;
		state.server->server->SendT(cancel);
	}

	walk_clear(state);
}

void
connection_walk_ack(Connection &c,
		    const struct uo_packet_walk_ack &p)
{
	auto &state = c.walk;

	auto *i = find_by_seq(state, p.seq);
	if (i == nullptr) {
		Log(1, "WalkAck out of sync\n");
		connection_resync(c);
		return;
	}

	LogFmt(7, "walk_ack seq_to_client={} seq_from_server={}\n",
	       i->packet.seq, p.seq);

	unsigned x = c.client.world.packet_start.x;
	unsigned y = c.client.world.packet_start.y;

	if ((c.client.world.packet_start.direction & 0x07) == (i->packet.direction & 0x07)) {
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

	c.client.world.Walked(x, y,
			      i->packet.direction, p.notoriety);

	/* forward ack to requesting client */
	if (state.server != nullptr) {
		auto ack = p;
		ack.seq = i->packet.seq;
		state.server->server->SendT(ack);
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
		&c.client.world.packet_mobile_update;

	if (state.server != nullptr)
		c.BroadcastToInGameClientsExcept(ReferenceAsBytes(mu),
						 *state.server);
	else
		c.BroadcastToInGameClients(ReferenceAsBytes(mu));

	remove_item(state, i);
}
