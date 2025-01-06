// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Connection.hxx"
#include "Handler.hxx"
#include "Instance.hxx"
#include "VerifyPacket.hxx"
#include "LinkedServer.hxx"
#include "Server.hxx"
#include "Client.hxx"
#include "Config.hxx"
#include "Log.hxx"
#include "version.h"
#include "Bridge.hxx"
#include "uo/Packets.hxx"
#include "net/SocketAddress.hxx"
#include "util/SpanCast.hxx"

#include <assert.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

void
Connection::Welcome()
{
	for (auto &ls : servers) {
		if (ls.IsInGame() && !ls.welcome) {
			ls.server->SpeakConsole("Welcome to uoproxy v" VERSION "!  "
						"https://github.com/MaxKellermann/uoproxy");
			ls.welcome = true;
		}
	}
}

/**
 * Send a HardwareInfo packet to the server, containing inane
 * information, to overwrite its old database entry.
 */
static void
send_antispy(UO::Client &client)
{
	static constexpr struct uo_packet_hardware p{
		.cmd = UO::Command::Hardware,
		.unknown0 = 2,
		.instance_id = 0xdeadbeef,
		.os_major = 5,
		.os_minor = 0,
		.os_revision = 0,
		.cpu_manufacturer = 3,
		.cpu_family = 6,
		.cpu_model = 8,
		.cpu_clock = 997,
		.cpu_quantity = 8,
		.physical_memory = 256,
		.screen_width = 1600,
		.screen_height = 1200,
		.screen_depth = 32,
		.dx_major = 9,
		.dx_minor = 0,
		.vc_description = {
			'S', 0, '3', 0, ' ', 0, 'T', 0, 'r', 0, 'i', 0, 'o',
		},
		.vc_vendor_id = 0,
		.vc_device_id = 0,
		.vc_memory = 4,
		.distribution = 2,
		.clients_running = 1,
		.clients_installed = 1,
		.partial_installed = 0,
		.language = { 'e', 0, 'n', 0, 'u', 0 },
		.unknown1 = {},
	};

	client.SendT(p);
}

PacketAction
Connection::HandleMobileStatus(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_mobile_status *>(src.data());

	client.world.Apply(*p);
	return PacketAction::ACCEPT;
}

PacketAction
Connection::HandleWorldItem(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_world_item *>(src.data());
	assert(src.size() <= sizeof(*p));

	client.world.Apply(*p);
	return PacketAction::ACCEPT;
}

PacketAction
Connection::HandleStart(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_start *>(src.data());
	assert(src.size() == sizeof(*p));

	client.world.packet_start = *p;

	/* if we're auto-reconnecting, this is the point where it
	   succeeded */
	client.reconnecting = false;

	walk.seq_next = 0;

	for (auto &ls : servers)
		ls.state = LinkedServer::State::IN_GAME;

	return PacketAction::ACCEPT;
}

PacketAction
Connection::HandleSpeakAscii([[maybe_unused]] std::span<const std::byte> src)
{
	Welcome();

	return PacketAction::ACCEPT;
}

PacketAction
Connection::HandleDelete(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_delete *>(src.data());
	assert(src.size() == sizeof(*p));

	client.world.RemoveSerial(p->serial);
	return PacketAction::ACCEPT;
}

PacketAction
Connection::HandleMobileUpdate(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_mobile_update *>(src.data());
	assert(src.size() == sizeof(*p));

	client.world.Apply(*p);
	return PacketAction::ACCEPT;
}

PacketAction
Connection::HandleWalkCancel(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_walk_cancel *>(src.data());
	assert(src.size() == sizeof(*p));

	if (!IsInGame())
		return PacketAction::DISCONNECT;

	OnWalkCancel(*p);
	return PacketAction::DROP;
}

PacketAction
Connection::HandleWalkAck(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_walk_ack *>(src.data());
	assert(src.size() == sizeof(*p));

	OnWalkAck(*p);

	/* XXX: x/y/z etc. */

	return PacketAction::DROP;
}

PacketAction
Connection::HandleContainerOpen(std::span<const std::byte> src)
{
	if (client.version.protocol >= ProtocolVersion::V7) {
		const auto *const p = reinterpret_cast<const struct uo_packet_container_open_7 *>(src.data());

		client.world.Apply(*p);

		BroadcastToInGameClientsDivert(ProtocolVersion::V7,
					       ReferenceAsBytes(p->base),
					       src);
		return PacketAction::DROP;
	} else {
		const auto *const p = reinterpret_cast<const struct uo_packet_container_open *>(src.data());
		assert(src.size() == sizeof(*p));

		client.world.Apply(*p);

		const struct uo_packet_container_open_7 p7 = {
			.base = *p,
			.zero = 0x00,
			.x7d = 0x7d,
		};

		BroadcastToInGameClientsDivert(ProtocolVersion::V7,
					       src,
					       ReferenceAsBytes(p7));

		return PacketAction::DROP;
	}
}

PacketAction
Connection::HandleContainerUpdate(std::span<const std::byte> src)
{
	if (client.version.protocol < ProtocolVersion::V6) {
		const auto *const p = reinterpret_cast<const struct uo_packet_container_update *>(src.data());
		struct uo_packet_container_update_6 p6;

		assert(src.size() == sizeof(*p));

		container_update_5_to_6(&p6, p);

		client.world.Apply(p6);

		BroadcastToInGameClientsDivert(ProtocolVersion::V6,
					       src,
					       ReferenceAsBytes(p6));
	} else {
		const auto *const p = reinterpret_cast<const struct uo_packet_container_update_6 *>(src.data());
		struct uo_packet_container_update p5;

		assert(src.size() == sizeof(*p));

		container_update_6_to_5(&p5, p);

		client.world.Apply(*p);

		BroadcastToInGameClientsDivert(ProtocolVersion::V6,
					       ReferenceAsBytes(p5),
					       src);
	}

	return PacketAction::DROP;
}

PacketAction
Connection::HandleEquip(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_equip *>(src.data());
	assert(src.size() == sizeof(*p));

	client.world.Apply(*p);
	return PacketAction::ACCEPT;
}

PacketAction
Connection::HandleContainerContent(std::span<const std::byte> src)
{
	if (packet_verify_container_content(reinterpret_cast<const struct uo_packet_container_content *>(src.data()), src.size())) {
		/* protocol v5 */
		const auto *const p = reinterpret_cast<const struct uo_packet_container_content *>(src.data());

		const auto p6 = container_content_5_to_6(p);

		client.world.Apply(*p6);

		BroadcastToInGameClientsDivert(ProtocolVersion::V6, src, p6);
	} else if (packet_verify_container_content_6(reinterpret_cast<const struct uo_packet_container_content_6 *>(src.data()), src.size())) {
		/* protocol v6 */
		const auto *const p = reinterpret_cast<const struct uo_packet_container_content_6 *>(src.data());

		client.world.Apply(*p);

		const auto p5 = container_content_6_to_5(p);
		BroadcastToInGameClientsDivert(ProtocolVersion::V6, p5, src);
	} else
		return PacketAction::DISCONNECT;

	return PacketAction::DROP;
}

PacketAction
Connection::HandlePersonalLightLevel(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_personal_light_level *>(src.data());
	assert(src.size() == sizeof(*p));

	if (instance.config.light)
		return PacketAction::DROP;

	if (client.world.packet_start.serial == p->serial)
		client.world.packet_personal_light_level = *p;

	return PacketAction::ACCEPT;
}

PacketAction
Connection::HandleGlobalLightLevel(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_global_light_level *>(src.data());
	assert(src.size() == sizeof(*p));

	if (instance.config.light)
		return PacketAction::DROP;

	client.world.packet_global_light_level = *p;
	return PacketAction::ACCEPT;
}

PacketAction
Connection::HandlePopupMessage(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_popup_message *>(src.data());

	assert(src.size() == sizeof(*p));

	if (client.reconnecting) {
		if (p->msg == 0x05) {
			SpeakConsole("previous character is still online, trying again");
		} else {
			SpeakConsole("character change failed, trying again");
		}

		ScheduleReconnect();
		return PacketAction::DELETED;
	}

	return PacketAction::ACCEPT;
}

PacketAction
Connection::HandleLoginComplete([[maybe_unused]] std::span<const std::byte> src)
{
	if (instance.config.antispy)
		send_antispy(*client.client);

	return PacketAction::ACCEPT;
}

PacketAction
Connection::HandleTarget(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_target *>(src.data());
	assert(src.size() == sizeof(*p));

	client.world.packet_target = *p;
	return PacketAction::ACCEPT;
}

PacketAction
Connection::HandleWarMode(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_war_mode *>(src.data());
	assert(src.size() == sizeof(*p));

	client.world.packet_war_mode = *p;
	return PacketAction::ACCEPT;
}

PacketAction
Connection::HandlePing(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_ping *>(src.data());
	assert(src.size() == sizeof(*p));

	client.ping_ack = p->id;
	return PacketAction::DROP;
}

PacketAction
Connection::HandleZoneChange(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_zone_change *>(src.data());
	assert(src.size() == sizeof(*p));

	client.world.Apply(*p);
	return PacketAction::ACCEPT;
}

PacketAction
Connection::HandleMobileMoving(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_mobile_moving *>(src.data());
	assert(src.size() == sizeof(*p));

	client.world.Apply(*p);
	return PacketAction::ACCEPT;
}

PacketAction
Connection::HandleMobileIncoming(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_mobile_incoming *>(src.data());

	if (src.size() < sizeof(*p) - sizeof(p->items))
		return PacketAction::DISCONNECT;

	client.world.Apply(*p);
	return PacketAction::ACCEPT;
}

PacketAction
Connection::HandleCharList(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_simple_character_list *>(src.data());

	/* save character list */
	if (p->character_count > 0 &&
	    src.size() >= sizeof(*p) + (p->character_count - 1) * sizeof(p->character_info[0]))
		client.char_list = {p, src.size()};

	/* respond directly during reconnect */
	if (client.reconnecting) {
		struct uo_packet_play_character p2 = {
			.cmd = UO::Command::PlayCharacter,
			.unknown0 = {},
			.name = {},
			.unknown1 = {},
			.flags = {},
			.unknown2 = {},
			.slot = character_index,
			.client_ip = 0xc0a80102, /* 192.168.1.2 */
		};

		Log(2, "sending PlayCharacter\n");

		client.client->SendT(p2);

		return PacketAction::DROP;
	} else {
		for (auto &ls : servers) {
			if (ls.state == LinkedServer::State::GAME_LOGIN ||
			    ls.state == LinkedServer::State::PLAY_SERVER) {
				ls.server->Send(src);
				ls.state = LinkedServer::State::CHAR_LIST;
			}
		}

		return PacketAction::DROP;
	}
}

PacketAction
Connection::HandleAccountLoginReject(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_account_login_reject *>(src.data());
	assert(src.size() == sizeof(*p));

	if (client.reconnecting) {
		LogFmt(1, "reconnect failed: AccountLoginReject reason={:#x}\n", p->reason);

		ScheduleReconnect();
		return PacketAction::DELETED;
	}

	if (IsInGame())
		return PacketAction::DISCONNECT;

	return PacketAction::ACCEPT;
}

PacketAction
Connection::HandleRelay(std::span<const std::byte> src)
{
	/* this packet tells the UO client where to connect; uoproxy hides
	   this packet from the client, and only internally connects to
	   the new server */
	const auto *const p = reinterpret_cast<const struct uo_packet_relay *>(src.data());
	struct uo_packet_relay relay;
	struct uo_packet_game_login login;

	assert(src.size() == sizeof(*p));

	if (IsInGame() && !client.reconnecting)
		return PacketAction::DISCONNECT;

	Log(2, "changing to game connection\n");

	/* save the relay packet - its buffer will be freed soon */
	relay = *p;

	/* close old connection */
	Disconnect();

	/* restore the "reconnecting" flag: it was cleared by
	   connection_client_disconnect(), but since continue reconnecting
	   on the new connection, we want to set it now.  the check at the
	   start of this function ensures that c.in_game is only set
	   during reconnecting. */

	client.reconnecting = IsInGame();

	/* extract new server's address */

	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = relay.port.raw();
	sin.sin_addr.s_addr = relay.ip.raw();

	/* connect to new server */

	if (client.version.seed != nullptr)
		client.version.seed->seed = relay.auth_id;

	try {
		Connect({(const struct sockaddr *)&sin, sizeof(sin)},
			relay.auth_id, true);
	} catch (...) {
		log_error("connect to game server failed", std::current_exception());
		return PacketAction::DISCONNECT;
	}

	/* send game login to new server */
	Log(2, "connected, doing GameLogin\n");

	login.cmd = UO::Command::GameLogin;
	login.auth_id = relay.auth_id;
	login.credentials = credentials;

	client.client->SendT(login);
	return PacketAction::DELETED;
}

PacketAction
Connection::HandleServerList(std::span<const std::byte> src)
{
	/* this packet tells the UO client where to connect; what
	   we do here is replace the server IP with our own one */
	const auto *const p = reinterpret_cast<const struct uo_packet_server_list *>(src.data());

	if (src.size() < sizeof(*p) || p->unknown_0x5d != 0x5d)
		return PacketAction::DISCONNECT;

	const unsigned count = p->num_game_servers;
	LogFmt(5, "serverlist: {} servers\n", count);

	const auto *server_info = p->game_servers;
	if (src.size() != sizeof(*p) + (count - 1) * sizeof(*server_info))
		return PacketAction::DISCONNECT;

	client.server_list = {p, src.size()};

	if (instance.config.antispy)
		send_antispy(*client.client);

	if (client.reconnecting) {
		struct uo_packet_play_server p2 = {
			.cmd = UO::Command::PlayServer,
			.index = 0, /* XXX */
		};

		client.client->SendT(p2);

		return PacketAction::DROP;
	}

	for (unsigned i = 0; i < count; i++, server_info++) {
		const unsigned k = server_info->index;
		if (k != i)
			return PacketAction::DISCONNECT;

		LogFmt(6, "server {}: name={:?} address={:#08x}\n",
		       (uint16_t)server_info->index,
		       server_info->name,
		       (uint32_t)server_info->address);
	}

	/* forward only to the clients which are waiting for the server
	   list (should be only one) */
	for (auto &ls : servers) {
		if (ls.state == LinkedServer::State::ACCOUNT_LOGIN) {
			ls.state = LinkedServer::State::SERVER_LIST;
			ls.server->Send(src);
		}
	}

	return PacketAction::DROP;
}

PacketAction
Connection::HandleSpeakUnicode([[maybe_unused]] std::span<const std::byte> src)
{
	Welcome();

	return PacketAction::ACCEPT;
}

PacketAction
Connection::HandleSupportedFeatures(std::span<const std::byte> src)
{
	if (client.version.protocol >= ProtocolVersion::V6_0_14) {
		const auto *const p = reinterpret_cast<const struct uo_packet_supported_features_6014 *>(src.data());
		assert(src.size() == sizeof(*p));

		client.supported_features_flags = p->flags;

		struct uo_packet_supported_features p6;
		supported_features_6014_to_6(&p6, p);
		BroadcastToInGameClientsDivert(ProtocolVersion::V6_0_14,
					       ReferenceAsBytes(p6),
					       src);
	} else {
		const auto *const p = reinterpret_cast<const struct uo_packet_supported_features *>(src.data());
		assert(src.size() == sizeof(*p));

		client.supported_features_flags = p->flags;

		struct uo_packet_supported_features_6014 p7;
		supported_features_6_to_6014(&p7, p);
		BroadcastToInGameClientsDivert(ProtocolVersion::V6_0_14,
					       src,
					       ReferenceAsBytes(p7));
	}

	return PacketAction::DROP;
}

PacketAction
Connection::HandleSeason(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_season *>(src.data());
	assert(src.size() == sizeof(*p));

	client.world.packet_season = *p;
	return PacketAction::ACCEPT;
}

PacketAction
Connection::HandleClientVersion([[maybe_unused]] std::span<const std::byte> src)
{
	if (client.version.IsDefined()) {
		LogFmt(3, "sending cached client version {:?}\n",
		       client.version.packet->version);

		/* respond to this packet directly if we know the version
		   number */
		client.client->Send(client.version.packet);
		return PacketAction::DROP;
	} else {
		/* we don't know the version - forward the request to all
		   clients */
		client.version_requested = true;
		return PacketAction::ACCEPT;
	}
}

PacketAction
Connection::HandleExtended(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_extended *>(src.data());
	if (src.size() < sizeof(*p))
		return PacketAction::DISCONNECT;

	LogFmt(8, "from server: extended {:#04x}\n", (uint16_t)p->extended_cmd);

	switch (p->extended_cmd) {
	case 0x0008:
		if (src.size() <= sizeof(client.world.packet_map_change))
			memcpy(&client.world.packet_map_change, src.data(), src.size());

		break;

	case 0x0018:
		if (src.size() <= sizeof(client.world.packet_map_patches))
			memcpy(&client.world.packet_map_patches, src.data(), src.size());
		break;
	}

	return PacketAction::ACCEPT;
}

PacketAction
Connection::HandleWorldItem7(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_world_item_7 *>(src.data());
	assert(src.size() == sizeof(*p));

	struct uo_packet_world_item old;
	world_item_from_7(&old, p);

	client.world.Apply(*p);

	BroadcastToInGameClientsDivert(ProtocolVersion::V7,
				       VarLengthAsBytes(old),
				       src);
	return PacketAction::DROP;
}

PacketAction
Connection::HandleProtocolExtension(std::span<const std::byte> src)
{
	const auto *const p = reinterpret_cast<const struct uo_packet_protocol_extension *>(src.data());
	if (src.size() < sizeof(*p))
		return PacketAction::DISCONNECT;

	if (p->extension == 0xfe) {
		/* BeginRazorHandshake; fake a response to avoid getting
		   kicked */

		struct uo_packet_protocol_extension response = {
			.cmd = UO::Command::ProtocolExtension,
			.length = sizeof(response),
			.extension = 0xff,
		};

		client.client->SendT(response);
		return PacketAction::DROP;
	}

	return PacketAction::ACCEPT;
}

constinit const Connection::CommandHandler Connection::command_handlers[] = {
	{ UO::Command::MobileStatus, &Connection::HandleMobileStatus }, /* 0x11 */
	{ UO::Command::WorldItem, &Connection::HandleWorldItem }, /* 0x1a */
	{ UO::Command::Start, &Connection::HandleStart }, /* 0x1b */
	{ UO::Command::SpeakAscii, &Connection::HandleSpeakAscii }, /* 0x1c */
	{ UO::Command::Delete, &Connection::HandleDelete }, /* 0x1d */
	{ UO::Command::MobileUpdate, &Connection::HandleMobileUpdate }, /* 0x20 */
	{ UO::Command::WalkCancel, &Connection::HandleWalkCancel }, /* 0x21 */
	{ UO::Command::WalkAck, &Connection::HandleWalkAck }, /* 0x22 */
	{ UO::Command::ContainerOpen, &Connection::HandleContainerOpen }, /* 0x24 */
	{ UO::Command::ContainerUpdate, &Connection::HandleContainerUpdate }, /* 0x25 */
	{ UO::Command::Equip, &Connection::HandleEquip }, /* 0x2e */
	{ UO::Command::ContainerContent, &Connection::HandleContainerContent }, /* 0x3c */
	{ UO::Command::PersonalLightLevel, &Connection::HandlePersonalLightLevel }, /* 0x4e */
	{ UO::Command::GlobalLightLevel, &Connection::HandleGlobalLightLevel }, /* 0x4f */
	{ UO::Command::PopupMessage, &Connection::HandlePopupMessage }, /* 0x53 */
	{ UO::Command::ReDrawAll, &Connection::HandleLoginComplete }, /* 0x55 */
	{ UO::Command::Target, &Connection::HandleTarget }, /* 0x6c */
	{ UO::Command::WarMode, &Connection::HandleWarMode }, /* 0x72 */
	{ UO::Command::Ping, &Connection::HandlePing }, /* 0x73 */
	{ UO::Command::ZoneChange, &Connection::HandleZoneChange }, /* 0x76 */
	{ UO::Command::MobileMoving, &Connection::HandleMobileMoving }, /* 0x77 */
	{ UO::Command::MobileIncoming, &Connection::HandleMobileIncoming }, /* 0x78 */
	{ UO::Command::CharList3, &Connection::HandleCharList }, /* 0x81 */
	{ UO::Command::AccountLoginReject, &Connection::HandleAccountLoginReject }, /* 0x82 */
	{ UO::Command::CharList2, &Connection::HandleCharList }, /* 0x86 */
	{ UO::Command::Relay, &Connection::HandleRelay }, /* 0x8c */
	{ UO::Command::ServerList, &Connection::HandleServerList }, /* 0xa8 */
	{ UO::Command::CharList, &Connection::HandleCharList }, /* 0xa9 */
	{ UO::Command::SpeakUnicode, &Connection::HandleSpeakUnicode }, /* 0xae */
	{ UO::Command::SupportedFeatures, &Connection::HandleSupportedFeatures }, /* 0xb9 */
	{ UO::Command::Season, &Connection::HandleSeason }, /* 0xbc */
	{ UO::Command::ClientVersion, &Connection::HandleClientVersion }, /* 0xbd */
	{ UO::Command::Extended, &Connection::HandleExtended }, /* 0xbf */
	{ UO::Command::WorldItem7, &Connection::HandleWorldItem7 }, /* 0xf3 */
	{ UO::Command::ProtocolExtension, &Connection::HandleProtocolExtension }, /* 0xf0 */
	{}
};

UO::PacketHandler::OnPacketResult
Connection::OnPacket(std::span<const std::byte> src)
{
	assert(client.client != nullptr);

	const auto action = DispatchPacket(*this, command_handlers, src);
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
		log_hexdump(6, src);

		if (autoreconnect && IsInGame()) {
			Log(2, "auto-reconnecting\n");
			ScheduleReconnect();
		} else {
			Destroy();
		}
		return OnPacketResult::CLOSED;

	case PacketAction::DELETED:
		return OnPacketResult::CLOSED;
	}

	return OnPacketResult::OK;
}
