// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Connection.hxx"
#include "LinkedServer.hxx"
#include "Instance.hxx"
#include "Server.hxx"
#include "Bridge.hxx"
#include "Log.hxx"

#include <assert.h>
#include <string.h>

static void
attach_item(LinkedServer *ls,
	    Item *item)
{
	World *world = &ls->connection->client.world;

	item->attach_sequence = world->item_attach_sequence;

	switch (item->socket.cmd) {
	case UO::Command::WorldItem:
		if (ls->client_version.protocol >= ProtocolVersion::V7) {
			ls->server->SendT(item->socket.ground);
		} else {
			struct uo_packet_world_item p;
			world_item_from_7(&p, &item->socket.ground);
			ls->server->Send(VarLengthAsBytes(p));
		}

		break;

	case UO::Command::ContainerUpdate:
		/* attach parent first */
		if (Item *parent = world->FindItem(item->socket.container.item.parent_serial);
		    parent != nullptr &&
		    parent->attach_sequence != world->item_attach_sequence)
			attach_item(ls, parent);

		/* then this item as container content */

		if (ls->client_version.protocol < ProtocolVersion::V6) {
			/* convert to v5 packet */
			struct uo_packet_container_update p5;

			container_update_6_to_5(&p5, &item->socket.container);
			ls->server->SendT(p5);
		} else {
			ls->server->SendT(item->socket.container);
		}

		break;

	case UO::Command::Equip:
		ls->server->SendT(item->socket.mobile);
		break;

	default:
		break;
	}

	if (item->packet_container_open.cmd == UO::Command::ContainerOpen) {
		if (ls->client_version.protocol >= ProtocolVersion::V7) {
			struct uo_packet_container_open_7 p7 = {
				.base = item->packet_container_open,
				.zero = 0x00,
				.x7d = 0x7d,
			};

			ls->server->SendT(p7);
		} else
			ls->server->SendT(item->packet_container_open);
	}
}

void
attach_send_world(LinkedServer *ls)
{
	World *world = &ls->connection->client.world;
	struct uo_packet_login_complete login_complete;

	/* 0x1b LoginConfirm */
	if (world->packet_start.cmd == UO::Command::Start)
		ls->server->SendT(world->packet_start);

	/* 0xbf 0x08 MapChange */
	if (world->packet_map_change.length > 0) {
		assert(world->packet_map_change.cmd == UO::Command::Extended);
		assert(world->packet_map_change.length == sizeof(world->packet_map_change));
		assert(world->packet_map_change.extended_cmd == 0x0008);
		ls->server->Send(VarLengthAsBytes(world->packet_map_change));
	}

	/* 0xbf 0x18 MapPatches */
	if (world->packet_map_patches.length > 0) {
		assert(world->packet_map_patches.cmd == UO::Command::Extended);
		assert(world->packet_map_patches.length == sizeof(world->packet_map_patches));
		assert(world->packet_map_patches.extended_cmd == 0x0018);
		ls->server->Send(VarLengthAsBytes(world->packet_map_patches));
	}

	/* 0xbc SeasonChange */
	if (world->packet_season.cmd == UO::Command::Season)
		ls->server->SendT(world->packet_season);

	/* 0xb9 SupportedFeatures */
	if (ls->client_version.protocol >= ProtocolVersion::V6_0_14) {
		struct uo_packet_supported_features_6014 supported_features;
		supported_features.cmd = UO::Command::SupportedFeatures;
		supported_features.flags = ls->connection->client.supported_features_flags;
		ls->server->SendT(supported_features);
	} else {
		struct uo_packet_supported_features supported_features;
		supported_features.cmd = UO::Command::SupportedFeatures;
		supported_features.flags = ls->connection->client.supported_features_flags;
		ls->server->SendT(supported_features);
	}

	/* 0x4f GlobalLightLevel */
	if (world->packet_global_light_level.cmd == UO::Command::GlobalLightLevel)
		ls->server->SendT(world->packet_global_light_level);

	/* 0x4e PersonalLightLevel */
	if (world->packet_personal_light_level.cmd == UO::Command::PersonalLightLevel)
		ls->server->SendT(world->packet_personal_light_level);

	/* 0x20 MobileUpdate */
	if (world->packet_mobile_update.cmd == UO::Command::MobileUpdate)
		ls->server->SendT(world->packet_mobile_update);

	/* WarMode */
	if (world->packet_war_mode.cmd == UO::Command::WarMode)
		ls->server->SendT(world->packet_war_mode);

	/* mobiles */
	for (const auto &mobile : world->mobiles) {
		if (mobile.packet_mobile_incoming != nullptr)
			ls->server->Send(mobile.packet_mobile_incoming);
		if (mobile.packet_mobile_status != nullptr)
			ls->server->Send(mobile.packet_mobile_status);
	}

	/* items */
	++world->item_attach_sequence;
	for (auto &item : world->items)
		if (item.attach_sequence != world->item_attach_sequence)
			attach_item(ls, &item);

	/* LoginComplete */
	login_complete.cmd = UO::Command::ReDrawAll;
	ls->server->SendT(login_complete);

	ls->state = LinkedServer::State::IN_GAME;
}
