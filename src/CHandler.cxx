// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Instance.hxx"
#include "PacketStructs.hxx"
#include "Handler.hxx"
#include "CVersion.hxx"
#include "Connection.hxx"
#include "LinkedServer.hxx"
#include "Client.hxx"
#include "Server.hxx"
#include "Config.hxx"
#include "Log.hxx"
#include "Bridge.hxx"
#include "UdpKnock.hxx"
#include "lib/fmt//SocketAddressFormatter.hxx"
#include "net/IPv4Address.hxx"
#include "net/SocketAddress.hxx"
#include "util/SpanCast.hxx"
#include "util/VarStructPtr.hxx"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <time.h>

#define TALK_MAX 128

static char *
simple_unicode_to_ascii(char *dest, const PackedBE16 *src,
			size_t length)
{
	size_t position;

	for (position = 0; position < length && src[position] != 0; position++) {
		uint16_t ch = src[position];
		if (ch & 0xff00)
			return nullptr;

		dest[position] = (char)ch;
	}

	dest[position] = 0;

	return dest;
}

static PacketAction
handle_talk(LinkedServer &ls, const char *text)
{
	/* the percent sign introduces an uoproxy command */
	if (text[0] == '%') {
		connection_handle_command(ls, text + 1);
		return PacketAction::DROP;
	}

	return PacketAction::ACCEPT;
}

static PacketAction
handle_create_character(LinkedServer &ls,
			std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_create_character *>(src.data());
	assert(src.size() == sizeof(*p));

	if (ls.connection->instance.config.antispy) {
		struct uo_packet_create_character q = *p;
		q.client_ip = 0xc0a80102;
		ls.connection->client.client->SendT(q);
		return PacketAction::DROP;
	} else
		return PacketAction::ACCEPT;
}

static PacketAction
handle_walk(LinkedServer &ls,
	    std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_walk *>(src.data());

	assert(src.size() == sizeof(*p));

	if (!ls.connection->IsInGame())
		return PacketAction::DISCONNECT;

	if (ls.connection->client.reconnecting) {
		World *world = &ls.connection->client.world;

		/* while reconnecting, reject all walk requests */
		struct uo_packet_walk_cancel p2 = {
			.cmd = UO::Command::WalkCancel,
			.seq = p->seq,
			.x = world->packet_start.x,
			.y = world->packet_start.y,
			.direction = world->packet_start.direction,
			.z = (int8_t)world->packet_start.z,
		};

		ls.server->SendT(p2);

		return PacketAction::DROP;
	}

	connection_walk_request(ls, *p);

	return PacketAction::DROP;
}

static PacketAction
handle_talk_ascii(LinkedServer &ls, std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_talk_ascii *>(src.data());

	if (src.size() < sizeof(*p))
		return PacketAction::DISCONNECT;

	const size_t text_length = src.size() - sizeof(*p);
	if (p->text[text_length] != 0)
		return PacketAction::DISCONNECT;

	return handle_talk(ls, p->text);
}

static PacketAction
handle_use(LinkedServer &ls,
	   std::span<const std::byte> src)
{
	[[maybe_unused]] const auto *const p = reinterpret_cast<const struct uo_packet_use *>(src.data());

	assert(src.size() == sizeof(*p));

	if (ls.connection->client.reconnecting) {
		ls.server->SpeakConsole("please wait until uoproxy finishes reconnecting");
		return PacketAction::DROP;
	}

#ifdef DUMP_USE
	do {
		Item *i = connection_find_item(c, p->serial);
		if (i == nullptr) {
			ls.LogF(7, "Use {:#x}", (uint32_t)p->serial);
		} else {
			uint16_t item_id;

			if (i->packet_world_item.cmd == UO::Command::WorldItem)
				item_id = i->packet_world_item.item_id;
			else if (i->packet_equip.cmd == UO::Command::Equip)
				item_id = i->packet_equip.item_id;
			else if (i->packet_container_update.cmd == UO::Command::ContainerUpdate)
				item_id = i->packet_container_update.item.item_id;
			else
				item_id = 0xffff;

			ls.LogF(7, "Use {:#x} item_id={#:x}",
				(uint32_t)p->serial, (uint32_t)item_id);
		}
		fflush(stdout);
	} while (0);
#endif

	return PacketAction::ACCEPT;
}

static PacketAction
handle_action(LinkedServer &ls, [[maybe_unused]] std::span<const std::byte> src)
{
	if (ls.connection->client.reconnecting) {
		ls.server->SpeakConsole("please wait until uoproxy finishes reconnecting");
		return PacketAction::DROP;
	}

	return PacketAction::ACCEPT;
}

static PacketAction
handle_lift_request(LinkedServer &ls,
		    std::span<const std::byte> src)
{
	[[maybe_unused]] const auto *const p = reinterpret_cast<const struct uo_packet_lift_request *>(src.data());

	assert(src.size() == sizeof(*p));

	if (ls.connection->client.reconnecting) {
		/* while reconnecting, reject all lift requests */
		struct uo_packet_lift_reject p2 = {
			.cmd = UO::Command::LiftReject,
			.reason = 0x00, /* CannotLift */
		};

		ls.server->SendT(p2);

		return PacketAction::DROP;
	}

	return PacketAction::ACCEPT;
}

static PacketAction
handle_drop(LinkedServer &ls,
	    std::span<const std::byte> src)
{
	auto *client = &ls.connection->client;

	if (!client->IsInGame() || client->reconnecting ||
	    client->client == nullptr)
		return PacketAction::DROP;

	if (ls.client_version.protocol < PROTOCOL_6) {
		const auto *const p = reinterpret_cast<const struct uo_packet_drop *>(src.data());

		assert(src.size() == sizeof(*p));

		if (ls.connection->client.version.protocol < PROTOCOL_6)
			return PacketAction::ACCEPT;

		struct uo_packet_drop_6 p6;
		drop_5_to_6(&p6, p);
		client->client->SendT(p6);
	} else {
		const auto *const p = reinterpret_cast<const struct uo_packet_drop_6 *>(src.data());

		assert(src.size() == sizeof(*p));

		if (ls.connection->client.version.protocol >= PROTOCOL_6)
			return PacketAction::ACCEPT;

		struct uo_packet_drop p5;
		drop_6_to_5(&p5, p);
		client->client->SendT(p5);
	}

	return PacketAction::DROP;
}

static PacketAction
handle_resynchronize(LinkedServer &ls, [[maybe_unused]] std::span<const std::byte> src)
{
	ls.LogF(3, "Resync!");

	ls.connection->walk.seq_next = 0;

	return PacketAction::ACCEPT;
}

static PacketAction
handle_target(LinkedServer &ls,
	      std::span<const std::byte> src)
{
	[[maybe_unused]] const auto *const p = reinterpret_cast<const struct uo_packet_target *>(src.data());
	World *world = &ls.connection->client.world;

	assert(src.size() == sizeof(*p));

	if (world->packet_target.cmd == UO::Command::Target &&
	    world->packet_target.target_id != 0) {
		/* cancel this target for all other clients */
		memset(&world->packet_target, 0,
		       sizeof(world->packet_target));
		world->packet_target.cmd = UO::Command::Target;
		world->packet_target.flags = 3;

		ls.connection->BroadcastToInGameClientsExcept(ReferenceAsBytes(world->packet_target),
							      ls);
	}


	if (p->allow_ground == 0 && p->serial == 0) {
		/* AFAICT, the only time we both don't allow ground
		   targetting and don't have a serial for a target is
		   when we are sending cancel target
		   requests.. otherwise what would the target be? */
	} else {
		ls.connection->walk.seq_next = 0;
		ls.connection->walk.queue_size = 0;
		ls.connection->walk.server = nullptr;
	}

	return PacketAction::ACCEPT;
}

static PacketAction
handle_ping(LinkedServer &ls, std::span<const std::byte> src)
{
	ls.server->Send(src);
	return PacketAction::DROP;
}

static PacketAction
handle_account_login(LinkedServer &ls, std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_account_login *>(src.data());
	Connection *c = ls.connection;
	const Config &config = c->instance.config;

	assert(src.size() == sizeof(*p));

	switch (ls.state) {
	case LinkedServer::State::INIT:
		break;

	case LinkedServer::State::ACCOUNT_LOGIN:
	case LinkedServer::State::SERVER_LIST:
	case LinkedServer::State::RELAY_SERVER:
	case LinkedServer::State::PLAY_SERVER:
	case LinkedServer::State::GAME_LOGIN:
	case LinkedServer::State::CHAR_LIST:
	case LinkedServer::State::PLAY_CHAR:
	case LinkedServer::State::IN_GAME:
		return PacketAction::DISCONNECT;
	}

	ls.state = LinkedServer::State::ACCOUNT_LOGIN;

#ifdef DUMP_LOGIN
	ls.LogF(7, "account_login: username={:?} password={:?}",
		p->username, p->password);
#endif

	if (c->client.client != nullptr) {
		ls.LogF(2, "already logged in");
		return PacketAction::DISCONNECT;
	}

	c->credentials = p->credentials;

	Connection *other = c->instance.FindAttachConnection(c->credentials);
	assert(other != c);
	if (other != nullptr) {
		if (other->client.server_list) {
			ls.server->Send(other->client.server_list);
			ls.state = LinkedServer::State::SERVER_LIST;
			return PacketAction::DROP;
		}

		/* attaching to an existing connection, fake the server
		   list */
		struct uo_packet_server_list p2;
		memset(&p2, 0, sizeof(p2));

		p2.cmd = UO::Command::ServerList;
		p2.length = sizeof(p2);
		p2.unknown_0x5d = 0x5d;
		p2.num_game_servers = 1;

		p2.game_servers[0].index = 0;
		strcpy(p2.game_servers[0].name, "attach");
		p2.game_servers[0].address = 0xdeadbeef;

		ls.server->SendT(p2);
		ls.state = LinkedServer::State::SERVER_LIST;
		return PacketAction::DROP;
	}

	if (!config.game_servers.empty()) {
		/* we have a game server list and thus we emulate the login
		   server */
		std::size_t i, num_game_servers = config.game_servers.size();

		struct uo_packet_server_list *p2;
		size_t length = sizeof(*p2) + (num_game_servers - 1) * sizeof(p2->game_servers[0]);

		const VarStructPtr<struct uo_packet_server_list> p2_(length);
		p2 = p2_.get();

		p2->cmd = UO::Command::ServerList;
		p2->length = length;
		p2->unknown_0x5d = 0x5d;
		p2->num_game_servers = num_game_servers;

		for (i = 0; i < num_game_servers; i++) {
			p2->game_servers[i].index = i;
			snprintf(p2->game_servers[i].name, sizeof(p2->game_servers[i].name),
				 "%s", config.game_servers[i].name.c_str());

			assert(!config.game_servers[i].address.IsNull());
			assert(config.game_servers[i].address.IsDefined());

			const SocketAddress _address{config.game_servers[i].address};
			if (_address.GetFamily() != AF_INET)
				continue;

			const auto &address = IPv4Address::Cast(_address);
			p2->game_servers[i].address = address.GetNumericAddressBE();
		}

		ls.server->Send(p2_);
		return PacketAction::DROP;
	} else if (!config.login_address.empty()) {
		/* connect to the real login server */
		uint32_t seed;

		if (config.antispy)
			/* since the login server seed usually contains the
			   internal IP address of the client, we want to hide it
			   in antispy mode - always send 192.168.1.2 which is
			   generic enough to be useless */
			seed = 0xc0a80102;
		else
			seed = ls.server->GetSeed();

		try {
			if (config.udp_knock && config.socks4_address.empty())
				SendUdpKnock(config.login_address.GetBest(), *p);

			c->Connect(config.login_address.GetBest(), seed, false);
		} catch (...) {
			log_error("connection to login server failed", std::current_exception());

			struct uo_packet_account_login_reject response;
			response.cmd = UO::Command::AccountLoginReject;
			response.reason = 0x02; /* blocked */

			ls.server->SendT(response);
			return PacketAction::DROP;
		}

		return PacketAction::ACCEPT;
	} else {
		/* should not happen */

		return PacketAction::DISCONNECT;
	}
}

static PacketAction
handle_game_login(LinkedServer &ls,
		  std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_game_login *>(src.data());

	assert(src.size() == sizeof(*p));

	if (!ls.connection->instance.config.razor_workaround)
		/* unless we're in Razor workaround mode, valid UO clients
		   will never send this packet since we're hiding redirects
		   from them */
		return PacketAction::DISCONNECT;

	bool find_zombie = false;

	switch (ls.state) {
	case LinkedServer::State::INIT:
		assert(!ls.connection->client.IsConnected());
		find_zombie = true;
		break;

	case LinkedServer::State::ACCOUNT_LOGIN:
	case LinkedServer::State::SERVER_LIST:
	case LinkedServer::State::PLAY_SERVER:
		return PacketAction::DISCONNECT;

	case LinkedServer::State::RELAY_SERVER:
		break;

	case LinkedServer::State::GAME_LOGIN:
	case LinkedServer::State::CHAR_LIST:
	case LinkedServer::State::PLAY_CHAR:
	case LinkedServer::State::IN_GAME:
		return PacketAction::DISCONNECT;
	}

	ls.state = LinkedServer::State::GAME_LOGIN;

	/* I have observed the Razor client ignoring the redirect if the IP
	   address differs from what it connected to.  (I guess this is a bug in
	   RunUO & Razor).  In that case it does a gamelogin on the old
	   linked_server without reconnecting to us.

	   So we apply the zombie-lookup only if the remote UO client actually
	   did bother to reconnet to us. */
	if (find_zombie) {
		auto &obsolete_connection = *ls.connection;
		auto &instance = obsolete_connection.instance;

		/* this should only happen in redirect mode.. so look for the
		   correct zombie so that we can re-use its connection to the UO
		   server. */
		LinkedServer *zombie = instance.FindZombie(*p);
		if (zombie == nullptr) {
			/* houston, we have a problem -- reject the game login -- it
			   either came in too slowly (and so we already reaped the
			   zombie) or it was a hack attempt (wrong password) */
			ls.LogF(2, "could not find previous connection for redirected client"
				" -- disconnecting client!");
			return PacketAction::DISCONNECT;
		}

		/* found it! Eureka! */
		auto &existing_connection = *zombie->connection;

		/* copy the previously detected protocol version */
		if (!existing_connection.IsInGame() &&
		    obsolete_connection.client.version.protocol != PROTOCOL_UNKNOWN)
			existing_connection.client.version.protocol = obsolete_connection.client.version.protocol;

		/* remove the object from the old connection */
		obsolete_connection.Remove(ls);
		obsolete_connection.Destroy();

		ls.LogF(2, "attaching redirected client to its previous connection");

		existing_connection.Add(ls);

		/* copy the protocol version from the old (zombie) client
		   connection; it is likely that we know this version already,
		   because AccountLogin is usually preceded by a Seed packet
		   which declares the protocol version, but the GameLogin
		   packet is not */
		if (ls.client_version.protocol == PROTOCOL_UNKNOWN &&
		    zombie->client_version.protocol != PROTOCOL_UNKNOWN) {
			ls.client_version.protocol = zombie->client_version.protocol;
			ls.server->SetProtocol(ls.client_version.protocol);
		}

		/* delete the zombie, we don't need it anymore */
		existing_connection.Remove(*zombie);
		delete zombie;
	}
	/* after GameLogin, must enable compression. */
	ls.server->SetCompression(true);
	if (ls.connection->IsInGame()) {
		/* already in game .. this was likely an attach connection */
		attach_send_world(&ls);
	} else if (ls.connection->client.char_list) {
		ls.server->Send(ls.connection->client.char_list);
		ls.state = LinkedServer::State::CHAR_LIST;
	}
	return PacketAction::DROP;
}

static PacketAction
handle_play_character(LinkedServer &ls,
		      std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_play_character *>(src.data());

	assert(src.size() == sizeof(*p));

	switch (ls.state) {
	case LinkedServer::State::INIT:
	case LinkedServer::State::ACCOUNT_LOGIN:
	case LinkedServer::State::SERVER_LIST:
	case LinkedServer::State::RELAY_SERVER:
	case LinkedServer::State::PLAY_SERVER:
	case LinkedServer::State::GAME_LOGIN:
		return PacketAction::DISCONNECT;

	case LinkedServer::State::CHAR_LIST:
		break;

	case LinkedServer::State::PLAY_CHAR:
	case LinkedServer::State::IN_GAME:
		return PacketAction::DISCONNECT;
	}

	ls.connection->character_index = p->slot;

	ls.state = LinkedServer::State::PLAY_CHAR;

	return PacketAction::ACCEPT;
}

static void
redirect_to_self(LinkedServer &ls)
{
	const auto local_ipv4 = ls.server->GetLocalIPv4Address();
	if (!local_ipv4.IsDefined())
		/* this connection was not IPv4 (maybe IPv6?) and we
                   can't send a redirect */
		return;

	ls.LogF(8, "redirecting to {}", local_ipv4);

	struct uo_packet_relay relay;
	static uint32_t authid = 0;

	if (!authid) authid = time(0);

	relay.cmd = UO::Command::Relay;
	relay.port = PackedBE16::FromBE(local_ipv4.GetNumericAddressBE());
	relay.ip = PackedBE32::FromBE(local_ipv4.GetPortBE());
	relay.auth_id = ls.auth_id = authid++;
	ls.server->SendT(relay);
	ls.state = LinkedServer::State::RELAY_SERVER;
}

static PacketAction
handle_play_server(LinkedServer &ls,
		   std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_play_server *>(src.data());
	auto &c = *ls.connection;
	auto &instance = c.instance;
	const auto &config = instance.config;
	PacketAction retaction = PacketAction::DROP;

	assert(src.size() == sizeof(*p));

	switch (ls.state) {
	case LinkedServer::State::INIT:
	case LinkedServer::State::RELAY_SERVER:
	case LinkedServer::State::ACCOUNT_LOGIN:
		return PacketAction::DISCONNECT;

	case LinkedServer::State::SERVER_LIST:
		break;

	case LinkedServer::State::PLAY_SERVER:
	case LinkedServer::State::GAME_LOGIN:
	case LinkedServer::State::CHAR_LIST:
	case LinkedServer::State::PLAY_CHAR:
	case LinkedServer::State::IN_GAME:
		return PacketAction::DISCONNECT;
	}

	ls.state = LinkedServer::State::PLAY_SERVER;

	assert(std::next(c.servers.iterator_to(ls)) == c.servers.end());

	c.server_index = p->index;

	auto *const c2 = instance.FindAttachConnection(c);
	if (c2 != nullptr) {
		/* remove the object from the old connection */
		c.Remove(ls);
		c.Destroy();

		c2->Add(ls);

		if (config.razor_workaround) { ///< need to send redirect
			/* attach it to the new connection and send redirect (below) */
		}  else {
			/* attach it to the new connection and begin playing right away */
			ls.LogF(2, "attaching connection");
			attach_send_world(&ls);
			return PacketAction::DROP;
		}

		retaction = PacketAction::DROP;
	} else if (config.login_address.empty() &&
		   !config.game_servers.empty()) {
		struct uo_packet_game_login login;
		uint32_t seed;

		assert(c.client.client == nullptr);

		/* locate the selected game server */
		const std::size_t i = p->index;
		if (i >= config.game_servers.size())
			return PacketAction::DISCONNECT;

		const auto &server_config = config.game_servers[i];

		/* connect to new server */

		if (c.client.version.seed != nullptr)
			seed = c.client.version.seed->seed;
		else
			seed = 0xc0a80102; /* 192.168.1.2 */

		try {
			c.Connect(server_config.address, seed, true);
		} catch (...) {
			log_error("connect to game server failed", std::current_exception());
			return PacketAction::DISCONNECT;
		}

		/* send game login to new server */
		login.cmd = UO::Command::GameLogin;
		login.auth_id = seed;
		login.credentials = c.credentials;

		c.client.client->SendT(login);

		retaction = PacketAction::DROP;
	} else
		retaction = PacketAction::ACCEPT;

	if (config.razor_workaround) {
		/* Razor workaround -> send the redirect packet to the client and tell
		   them to redirect to self!  This is because Razor refuses to work if
		   it doesn't see a redirect packet.  Note that after the redirect,
		   the client immediately sends 'GameLogin' which means we turn
		   compression on. */
		redirect_to_self(ls);
	}

	return retaction;
}

static PacketAction
handle_spy(LinkedServer &ls, [[maybe_unused]] std::span<const std::byte> src)
{
	if (ls.connection->instance.config.antispy)
		return PacketAction::DROP;

	return PacketAction::ACCEPT;
}

static PacketAction
handle_talk_unicode(LinkedServer &ls, std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_talk_unicode *>(src.data());

	if (src.size() < sizeof(*p))
		return PacketAction::DISCONNECT;

	if (p->type == 0xc0) {
		uint16_t value = p->text[0];
		unsigned num_keywords = (value & 0xfff0) >> 4;
		unsigned skip_bits = (num_keywords + 1) * 12;
		unsigned skip_bytes = 12 + (skip_bits + 7) / 8;
		const char *start = (const char *)src.data();
		const char *t = start + skip_bytes;
		size_t text_length = src.size() - skip_bytes - 1;

		if (skip_bytes >= src.size())
			return PacketAction::DISCONNECT;

		if (t[0] == 0 || t[text_length] != 0)
			return PacketAction::DISCONNECT;

		/* the text may be UTF-8, but we ignore that for now */
		return handle_talk(ls, t);
	} else {
		size_t text_length = (src.size() - sizeof(*p)) / 2;

		if (text_length < TALK_MAX) { /* Regular */
			char msg[TALK_MAX], *t;

			t = simple_unicode_to_ascii(msg, p->text, text_length);
			if (t != nullptr)
				return handle_talk(ls, t);
		}
	}

	return PacketAction::ACCEPT;
}

static PacketAction
handle_gump_response(LinkedServer &ls, std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_gump_response *>(src.data());

	if (src.size() < sizeof(*p))
		return PacketAction::DISCONNECT;

	/* close the gump on all other clients */
	const struct uo_packet_close_gump close = {
		.cmd = UO::Command::Extended,
		.length = sizeof(close),
		.extended_cmd = 0x0004,
		.type_id = p->type_id,
		.button_id = 0,
	};

	ls.connection->BroadcastToInGameClientsExcept(ReferenceAsBytes(close), ls);

	return PacketAction::ACCEPT;
}

static PacketAction
handle_client_version(LinkedServer &ls, std::span<const std::byte> src)
{
	Connection *c = ls.connection;
	const auto *const p = reinterpret_cast<const struct uo_packet_client_version *>(src.data());

	if (!ls.client_version.IsDefined()) {
		bool was_unkown = ls.client_version.protocol == PROTOCOL_UNKNOWN;
		int ret = ls.client_version.Set(p, src.size());
		if (ret > 0) {
			if (was_unkown)
				ls.server->SetProtocol(ls.client_version.protocol);

			ls.LogF(2, "client version {:?}, protocol {:?}",
				ls.client_version.packet->version,
				protocol_name(ls.client_version.protocol));
		}
	}

	if (c->client.version.IsDefined()) {
		if (c->client.version_requested) {
			c->client.client->Send(c->client.version.packet);
			c->client.version_requested = false;
		}

		return PacketAction::DROP;
	} else {
		const bool was_unkown = c->client.version.protocol == PROTOCOL_UNKNOWN;

		int ret = c->client.version.Set(p, src.size());
		if (ret > 0) {
			if (was_unkown && c->client.client != nullptr)
				c->client.client->SetProtocol(c->client.version.protocol);
			ls.LogF(2, "emulating client version {:?}, protocol {:?}",
				c->client.version.packet->version,
				protocol_name(c->client.version.protocol));
		} else if (ret == 0)
			ls.LogF(2, "invalid client version");
		return PacketAction::ACCEPT;
	}
}

static PacketAction
handle_extended(LinkedServer &ls, std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_extended *>(src.data());

	if (src.size() < sizeof(*p))
		return PacketAction::DISCONNECT;

	ls.LogF(8, "from client: extended {:#04x}", (unsigned)p->extended_cmd);

	return PacketAction::ACCEPT;
}

static PacketAction
handle_hardware(LinkedServer &ls, [[maybe_unused]] std::span<const std::byte> src)
{
	if (ls.connection->instance.config.antispy)
		return PacketAction::DROP;

	return PacketAction::ACCEPT;
}

static PacketAction
handle_seed(LinkedServer &ls, std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_seed *>(src.data());

	assert(src.size() == sizeof(*p));

	if (ls.client_version.seed == nullptr) {
		ls.client_version.Seed(*p);
		ls.server->SetProtocol(ls.client_version.protocol);

		ls.LogF(2, "detected client 6.0.5.0 or newer ({}.{}.{}.{})",
			(unsigned)p->client_major,
			(unsigned)p->client_minor,
			(unsigned)p->client_revision,
			(unsigned)p->client_patch);
	}

	if (!ls.connection->client.version.IsDefined() &&
	    ls.connection->client.version.seed == nullptr) {
		ls.connection->client.version.Seed(*p);

		if (ls.connection->client.client != nullptr)
			ls.connection->client.client->SetProtocol(ls.connection->client.version.protocol);
	}

	return PacketAction::DROP;
}

const struct server_packet_binding client_packet_bindings[] = {
	{ UO::Command::CreateCharacter, handle_create_character },
	{ UO::Command::Walk, handle_walk },
	{ UO::Command::TalkAscii, handle_talk_ascii },
	{ UO::Command::Use, handle_use },
	{ UO::Command::Action, handle_action },
	{ UO::Command::LiftRequest, handle_lift_request },
	{ UO::Command::Drop, handle_drop }, /* 0x08 */
	{ UO::Command::Resynchronize, handle_resynchronize },
	{ UO::Command::Target, handle_target }, /* 0x6c */
	{ UO::Command::Ping, handle_ping },
	{ UO::Command::AccountLogin, handle_account_login },
	{ UO::Command::AccountLogin2, handle_account_login },
	{ UO::Command::GameLogin, handle_game_login },
	{ UO::Command::PlayCharacter, handle_play_character },
	{ UO::Command::PlayServer, handle_play_server },
	{ UO::Command::Spy, handle_spy }, /* 0xa4 */
	{ UO::Command::TalkUnicode, handle_talk_unicode },
	{ UO::Command::GumpResponse, handle_gump_response },
	{ UO::Command::ClientVersion, handle_client_version }, /* 0xbd */
	{ UO::Command::Extended, handle_extended },
	{ UO::Command::Hardware, handle_hardware }, /* 0xd9 */
	{ UO::Command::Seed, handle_seed }, /* 0xef */
	{}
};
