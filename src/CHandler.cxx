// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "LinkedServer.hxx"
#include "Attach.hxx"
#include "Instance.hxx"
#include "PacketStructs.hxx"
#include "Handler.hxx"
#include "CVersion.hxx"
#include "Connection.hxx"
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

static std::string_view
simple_unicode_to_ascii(char *dest, const PackedBE16 *src,
			size_t length)
{
	size_t position;

	for (position = 0; position < length && src[position] != 0; position++) {
		uint16_t ch = src[position];
		if (ch & 0xff00)
			return {};

		dest[position] = (char)ch;
	}

	return {dest, position};
}

inline PacketAction
LinkedServer::HandleTalk(std::string_view text)
{
	/* the percent sign introduces an uoproxy command */
	if (text.starts_with('%')) {
		OnCommand(text.substr(1));
		return PacketAction::DROP;
	}

	return PacketAction::ACCEPT;
}

PacketAction
LinkedServer::HandleCreateCharacter(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_create_character *>(src.data());
	assert(src.size() == sizeof(*p));

	if (connection->instance.config.antispy) {
		struct uo_packet_create_character q = *p;
		q.client_ip = 0xc0a80102;
		connection->client.client->SendT(q);
		return PacketAction::DROP;
	} else
		return PacketAction::ACCEPT;
}

PacketAction
LinkedServer::HandleWalk(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_walk *>(src.data());

	assert(src.size() == sizeof(*p));

	if (!connection->IsInGame())
		return PacketAction::DISCONNECT;

	if (connection->client.reconnecting) {
		World *world = &connection->client.world;

		/* while reconnecting, reject all walk requests */
		struct uo_packet_walk_cancel p2 = {
			.cmd = UO::Command::WalkCancel,
			.seq = p->seq,
			.x = world->packet_start.x,
			.y = world->packet_start.y,
			.direction = world->packet_start.direction,
			.z = (int8_t)world->packet_start.z,
		};

		server->SendT(p2);

		return PacketAction::DROP;
	}

	OnWalkRequest(*p);
	return PacketAction::DROP;
}

PacketAction
LinkedServer::HandleTalkAscii(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_talk_ascii *>(src.data());
	if (src.size() < sizeof(*p))
		return PacketAction::DISCONNECT;

	const size_t text_length = src.size() - sizeof(*p);
	if (p->text[text_length] != 0)
		return PacketAction::DISCONNECT;

	return HandleTalk({p->text, text_length});
}

PacketAction
LinkedServer::HandleUse(std::span<const std::byte> src)
{
	[[maybe_unused]] const auto *const p = reinterpret_cast<const struct uo_packet_use *>(src.data());
	assert(src.size() == sizeof(*p));

	if (connection->client.reconnecting) {
		server->SpeakConsole("please wait until uoproxy finishes reconnecting");
		return PacketAction::DROP;
	}

#ifdef DUMP_USE
	do {
		Item *i = connection_find_item(c, p->serial);
		if (i == nullptr) {
			LogF(7, "Use {:#x}", (uint32_t)p->serial);
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

			LogF(7, "Use {:#x} item_id={#:x}",
				(uint32_t)p->serial, (uint32_t)item_id);
		}
		fflush(stdout);
	} while (0);
#endif

	return PacketAction::ACCEPT;
}

PacketAction
LinkedServer::HandleAction([[maybe_unused]] std::span<const std::byte> src)
{
	if (connection->client.reconnecting) {
		server->SpeakConsole("please wait until uoproxy finishes reconnecting");
		return PacketAction::DROP;
	}

	return PacketAction::ACCEPT;
}

PacketAction
LinkedServer::HandleLiftRequest(std::span<const std::byte> src)
{
	[[maybe_unused]] const auto *const p = reinterpret_cast<const struct uo_packet_lift_request *>(src.data());
	assert(src.size() == sizeof(*p));

	if (connection->client.reconnecting) {
		/* while reconnecting, reject all lift requests */
		struct uo_packet_lift_reject p2 = {
			.cmd = UO::Command::LiftReject,
			.reason = 0x00, /* CannotLift */
		};

		server->SendT(p2);
		return PacketAction::DROP;
	}

	return PacketAction::ACCEPT;
}

PacketAction
LinkedServer::HandleDrop(std::span<const std::byte> src)
{
	auto *client = &connection->client;

	if (!client->IsInGame() || client->reconnecting ||
	    client->client == nullptr)
		return PacketAction::DROP;

	if (client_version.protocol < ProtocolVersion::V6) {
		const auto *const p = reinterpret_cast<const struct uo_packet_drop *>(src.data());

		assert(src.size() == sizeof(*p));

		if (connection->client.version.protocol < ProtocolVersion::V6)
			return PacketAction::ACCEPT;

		struct uo_packet_drop_6 p6;
		drop_5_to_6(&p6, p);
		client->client->SendT(p6);
	} else {
		const auto *const p = reinterpret_cast<const struct uo_packet_drop_6 *>(src.data());

		assert(src.size() == sizeof(*p));

		if (connection->client.version.protocol >= ProtocolVersion::V6)
			return PacketAction::ACCEPT;

		struct uo_packet_drop p5;
		drop_6_to_5(&p5, p);
		client->client->SendT(p5);
	}

	return PacketAction::DROP;
}

PacketAction
LinkedServer::HandleResynchronize([[maybe_unused]] std::span<const std::byte> src)
{
	LogF(3, "Resync!");
	connection->walk.seq_next = 0;
	return PacketAction::ACCEPT;
}

PacketAction
LinkedServer::HandleTarget(std::span<const std::byte> src)
{
	[[maybe_unused]] const auto *const p = reinterpret_cast<const struct uo_packet_target *>(src.data());
	World *world = &connection->client.world;

	assert(src.size() == sizeof(*p));

	if (world->packet_target.cmd == UO::Command::Target &&
	    world->packet_target.target_id != 0) {
		/* cancel this target for all other clients */
		memset(&world->packet_target, 0,
		       sizeof(world->packet_target));
		world->packet_target.cmd = UO::Command::Target;
		world->packet_target.flags = 3;

		connection->BroadcastToInGameClientsExcept(ReferenceAsBytes(world->packet_target),
							   *this);
	}


	if (p->allow_ground == 0 && p->serial == 0) {
		/* AFAICT, the only time we both don't allow ground
		   targetting and don't have a serial for a target is
		   when we are sending cancel target
		   requests.. otherwise what would the target be? */
	} else {
		connection->walk.seq_next = 0;
		connection->walk.queue_size = 0;
		connection->walk.server = nullptr;
	}

	return PacketAction::ACCEPT;
}

PacketAction
LinkedServer::HandlePing(std::span<const std::byte> src)
{
	server->Send(src);
	return PacketAction::DROP;
}

PacketAction
LinkedServer::HandleAccountLogin(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_account_login *>(src.data());
	Connection *c = connection;
	const Config &config = c->instance.config;

	assert(src.size() == sizeof(*p));

	switch (state) {
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

	state = LinkedServer::State::ACCOUNT_LOGIN;

#ifdef DUMP_LOGIN
	LogF(7, "account_login: username={:?} password={:?}",
	     p->username, p->password);
#endif

	if (c->client.client != nullptr) {
		LogF(2, "already logged in");
		return PacketAction::DISCONNECT;
	}

	c->credentials = p->credentials;

	Connection *other = c->instance.FindAttachConnection(c->credentials);
	assert(other != c);
	if (other != nullptr) {
		if (other->client.server_list) {
			server->Send(other->client.server_list);
			state = LinkedServer::State::SERVER_LIST;
			return PacketAction::DROP;
		}

		/* attaching to an existing connection, fake the server
		   list */
		struct uo_packet_server_list p2{
			.cmd = UO::Command::ServerList,
			.length = sizeof(p2),
			.unknown_0x5d = 0x5d,
			.num_game_servers = 1,
		};

		p2.game_servers[0].index = 0;
		strcpy(p2.game_servers[0].name, "attach");
		p2.game_servers[0].address = 0xdeadbeef;

		server->SendT(p2);
		state = LinkedServer::State::SERVER_LIST;
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

		server->Send(p2_);
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
			seed = server->GetSeed();

		try {
			if (config.udp_knock && config.socks4_address.empty())
				SendUdpKnock(config.login_address.GetBest(), *p);

			c->Connect(config.login_address.GetBest(), seed, false);
		} catch (...) {
			log_error("connection to login server failed", std::current_exception());

			static constexpr struct uo_packet_account_login_reject response{
				.cmd = UO::Command::AccountLoginReject,
				.reason = 0x02, /* blocked */
			};

			server->SendT(response);
			return PacketAction::DROP;
		}

		return PacketAction::ACCEPT;
	} else {
		/* should not happen */

		return PacketAction::DISCONNECT;
	}
}

PacketAction
LinkedServer::HandleGameLogin(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_game_login *>(src.data());

	assert(src.size() == sizeof(*p));

	if (!connection->instance.config.razor_workaround)
		/* unless we're in Razor workaround mode, valid UO clients
		   will never send this packet since we're hiding redirects
		   from them */
		return PacketAction::DISCONNECT;

	bool find_zombie = false;

	switch (state) {
	case LinkedServer::State::INIT:
		assert(!connection->client.IsConnected());
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

	state = LinkedServer::State::GAME_LOGIN;

	/* I have observed the Razor client ignoring the redirect if the IP
	   address differs from what it connected to.  (I guess this is a bug in
	   RunUO & Razor).  In that case it does a gamelogin on the old
	   linked_server without reconnecting to us.

	   So we apply the zombie-lookup only if the remote UO client actually
	   did bother to reconnet to us. */
	if (find_zombie) {
		auto &obsolete_connection = *connection;
		auto &instance = obsolete_connection.instance;

		/* this should only happen in redirect mode.. so look for the
		   correct zombie so that we can re-use its connection to the UO
		   server. */
		LinkedServer *zombie = instance.FindZombie(*p);
		if (zombie == nullptr) {
			/* houston, we have a problem -- reject the game login -- it
			   either came in too slowly (and so we already reaped the
			   zombie) or it was a hack attempt (wrong password) */
			LogF(2, "could not find previous connection for redirected client"
			     " -- disconnecting client!");
			return PacketAction::DISCONNECT;
		}

		/* found it! Eureka! */
		auto &existing_connection = *zombie->connection;

		/* copy the previously detected protocol version */
		if (!existing_connection.IsInGame() &&
		    obsolete_connection.client.version.protocol != ProtocolVersion::UNKNOWN)
			existing_connection.client.version.protocol = obsolete_connection.client.version.protocol;

		/* remove the object from the old connection */
		obsolete_connection.Remove(*this);
		obsolete_connection.Destroy();

		LogF(2, "attaching redirected client to its previous connection");

		existing_connection.Add(*this);

		/* copy the protocol version from the old (zombie) client
		   connection; it is likely that we know this version already,
		   because AccountLogin is usually preceded by a Seed packet
		   which declares the protocol version, but the GameLogin
		   packet is not */
		if (client_version.protocol == ProtocolVersion::UNKNOWN &&
		    zombie->client_version.protocol != ProtocolVersion::UNKNOWN) {
			client_version.protocol = zombie->client_version.protocol;
			server->SetProtocol(client_version.protocol);
		}

		/* delete the zombie, we don't need it anymore */
		existing_connection.Remove(*zombie);
		delete zombie;
	}
	/* after GameLogin, must enable compression. */
	server->SetCompression(true);
	if (connection->IsInGame()) {
		/* already in game .. this was likely an attach connection */
		SendWorld(*server, client_version.protocol,
			  connection->client.supported_features_flags,
			  connection->client.world);
		state = LinkedServer::State::IN_GAME;
	} else if (connection->client.char_list) {
		server->Send(connection->client.char_list);
		state = LinkedServer::State::CHAR_LIST;
	}
	return PacketAction::DROP;
}

PacketAction
LinkedServer::HandlePlayCharacter(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_play_character *>(src.data());

	assert(src.size() == sizeof(*p));

	switch (state) {
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

	connection->character_index = p->slot;
	state = LinkedServer::State::PLAY_CHAR;
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

	static uint32_t authid = 0;

	if (!authid) authid = time(0);

	const struct uo_packet_relay relay{
		.cmd = UO::Command::Relay,
		.ip = PackedBE32::FromBE(local_ipv4.GetPortBE()),
		.port = PackedBE16::FromBE(local_ipv4.GetNumericAddressBE()),
		.auth_id = authid++,
	};
		
	ls.auth_id = relay.auth_id;

	ls.server->SendT(relay);
	ls.state = LinkedServer::State::RELAY_SERVER;
}

PacketAction
LinkedServer::HandlePlayServer(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_play_server *>(src.data());
	auto &c = *connection;
	auto &instance = c.instance;
	const auto &config = instance.config;
	PacketAction retaction = PacketAction::DROP;

	assert(src.size() == sizeof(*p));

	switch (state) {
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

	state = LinkedServer::State::PLAY_SERVER;

	assert(std::next(c.servers.iterator_to(*this)) == c.servers.end());

	c.server_index = p->index;

	auto *const c2 = instance.FindAttachConnection(c);
	if (c2 != nullptr) {
		/* remove the object from the old connection */
		c.Remove(*this);
		c.Destroy();

		c2->Add(*this);

		if (config.razor_workaround) { ///< need to send redirect
			/* attach it to the new connection and send redirect (below) */
		}  else {
			/* attach it to the new connection and begin playing right away */
			LogF(2, "attaching connection");
			SendWorld(*server, client_version.protocol,
				  connection->client.supported_features_flags,
				  connection->client.world);
			state = LinkedServer::State::IN_GAME;
			return PacketAction::DROP;
		}

		retaction = PacketAction::DROP;
	} else if (config.login_address.empty() &&
		   !config.game_servers.empty()) {
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
		const struct uo_packet_game_login login{
			.cmd = UO::Command::GameLogin,
			.auth_id = seed,
			.credentials = c.credentials,
		};

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
		redirect_to_self(*this);
	}

	return retaction;
}

PacketAction
LinkedServer::HandleSpy([[maybe_unused]] std::span<const std::byte> src)
{
	if (connection->instance.config.antispy)
		return PacketAction::DROP;

	return PacketAction::ACCEPT;
}

PacketAction
LinkedServer::HandleTalkUnicode(std::span<const std::byte> src)
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
		return HandleTalk({t, text_length});
	} else {
		size_t text_length = (src.size() - sizeof(*p)) / 2;

		if (text_length < TALK_MAX) { /* Regular */
			char msg[TALK_MAX];

			const auto t = simple_unicode_to_ascii(msg, p->text, text_length);
			if (t.data() != nullptr)
				return HandleTalk(t);
		}
	}

	return PacketAction::ACCEPT;
}

PacketAction
LinkedServer::HandleGumpResponse(std::span<const std::byte> src)
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

	connection->BroadcastToInGameClientsExcept(ReferenceAsBytes(close), *this);
	return PacketAction::ACCEPT;
}

PacketAction
LinkedServer::HandleClientVersion(std::span<const std::byte> src)
{
	Connection *c = connection;
	const auto *const p = reinterpret_cast<const struct uo_packet_client_version *>(src.data());

	if (!client_version.IsDefined()) {
		bool was_unkown = client_version.protocol == ProtocolVersion::UNKNOWN;
		int ret = client_version.Set(p, src.size());
		if (ret > 0) {
			if (was_unkown)
				server->SetProtocol(client_version.protocol);

			LogF(2, "client version {:?}, protocol {:?}",
			     client_version.packet->version,
			     protocol_name(client_version.protocol));
		}
	}

	if (c->client.version.IsDefined()) {
		if (c->client.version_requested) {
			c->client.client->Send(c->client.version.packet);
			c->client.version_requested = false;
		}

		return PacketAction::DROP;
	} else {
		const bool was_unkown = c->client.version.protocol == ProtocolVersion::UNKNOWN;

		int ret = c->client.version.Set(p, src.size());
		if (ret > 0) {
			if (was_unkown && c->client.client != nullptr)
				c->client.client->SetProtocol(c->client.version.protocol);
			LogF(2, "emulating client version {:?}, protocol {:?}",
			     c->client.version.packet->version,
			     protocol_name(c->client.version.protocol));
		} else if (ret == 0)
			LogF(2, "invalid client version");
		return PacketAction::ACCEPT;
	}
}

PacketAction
LinkedServer::HandleExtended(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_extended *>(src.data());
	if (src.size() < sizeof(*p))
		return PacketAction::DISCONNECT;

	LogF(8, "from client: extended {:#04x}", (unsigned)p->extended_cmd);
	return PacketAction::ACCEPT;
}

PacketAction
LinkedServer::HandleHardware([[maybe_unused]] std::span<const std::byte> src)
{
	if (connection->instance.config.antispy)
		return PacketAction::DROP;

	return PacketAction::ACCEPT;
}

PacketAction
LinkedServer::HandleSeed(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_seed *>(src.data());

	assert(src.size() == sizeof(*p));

	if (client_version.seed == nullptr) {
		client_version.Seed(*p);
		server->SetProtocol(client_version.protocol);

		LogF(2, "detected client 6.0.5.0 or newer ({}.{}.{}.{})",
		     (unsigned)p->client_major,
		     (unsigned)p->client_minor,
		     (unsigned)p->client_revision,
		     (unsigned)p->client_patch);
	}

	if (!connection->client.version.IsDefined() &&
	    connection->client.version.seed == nullptr) {
		connection->client.version.Seed(*p);

		if (connection->client.client != nullptr)
			connection->client.client->SetProtocol(connection->client.version.protocol);
	}

	return PacketAction::DROP;
}

struct server_packet_binding {
	UO::Command cmd;
	PacketAction (*handler)(LinkedServer &ls,
				std::span<const std::byte> src);
};

constinit const LinkedServer::CommandHandler LinkedServer::command_handlers[] = {
	{ UO::Command::CreateCharacter, &LinkedServer::HandleCreateCharacter },
	{ UO::Command::Walk, &LinkedServer::HandleWalk },
	{ UO::Command::TalkAscii, &LinkedServer::HandleTalkAscii },
	{ UO::Command::Use, &LinkedServer::HandleUse },
	{ UO::Command::Action, &LinkedServer::HandleAction },
	{ UO::Command::LiftRequest, &LinkedServer::HandleLiftRequest },
	{ UO::Command::Drop, &LinkedServer::HandleDrop }, /* 0x08 */
	{ UO::Command::Resynchronize, &LinkedServer::HandleResynchronize },
	{ UO::Command::Target, &LinkedServer::HandleTarget }, /* 0x6c */
	{ UO::Command::Ping, &LinkedServer::HandlePing },
	{ UO::Command::AccountLogin, &LinkedServer::HandleAccountLogin },
	{ UO::Command::AccountLogin2, &LinkedServer::HandleAccountLogin },
	{ UO::Command::GameLogin, &LinkedServer::HandleGameLogin },
	{ UO::Command::PlayCharacter, &LinkedServer::HandlePlayCharacter },
	{ UO::Command::PlayServer, &LinkedServer::HandlePlayServer },
	{ UO::Command::Spy, &LinkedServer::HandleSpy }, /* 0xa4 */
	{ UO::Command::TalkUnicode, &LinkedServer::HandleTalkUnicode },
	{ UO::Command::GumpResponse, &LinkedServer::HandleGumpResponse },
	{ UO::Command::ClientVersion, &LinkedServer::HandleClientVersion }, /* 0xbd */
	{ UO::Command::Extended, &LinkedServer::HandleExtended },
	{ UO::Command::Hardware, &LinkedServer::HandleHardware }, /* 0xd9 */
	{ UO::Command::Seed, &LinkedServer::HandleSeed }, /* 0xef */
	{}
};

bool
LinkedServer::OnPacket(std::span<const std::byte> src)
{
	Connection *c = connection;

	assert(c != nullptr);
	assert(server != nullptr);

	const auto action = DispatchPacket(*this, command_handlers, src);
	switch (action) {
	case PacketAction::ACCEPT:
		if (c->client.client != nullptr &&
		    (!c->client.reconnecting ||
		     static_cast<UO::Command>(src.front()) == UO::Command::ClientVersion))
			c->client.client->Send(src);
		break;

	case PacketAction::DROP:
		break;

	case PacketAction::DISCONNECT:
		LogF(2, "aborting connection to client after packet {:#x}", src.front());
		log_hexdump(6, src);

		c->Remove(*this);

		if (c->servers.empty()) {
			if (c->background)
				LogF(1, "backgrounding");
			else
				c->Destroy();
		}

		delete this;
		return false;

	case PacketAction::DELETED:
		return false;
	}

	return true;
}
