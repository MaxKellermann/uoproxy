// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Attach.hxx"
#include "Server.hxx"
#include "Bridge.hxx"
#include "World.hxx"

#include <cassert>

static void
AttachItem(UO::Server &server, ProtocolVersion protocol_version,
	   World &world, Item &item)
{
	item.attach_sequence = world.item_attach_sequence;

	switch (item.socket.cmd) {
	case UO::Command::WorldItem:
		if (protocol_version >= ProtocolVersion::V7) {
			server.SendT(item.socket.ground);
		} else {
			struct uo_packet_world_item p;
			world_item_from_7(&p, &item.socket.ground);
			server.Send(VarLengthAsBytes(p));
		}

		break;

	case UO::Command::ContainerUpdate:
		/* attach parent first */
		if (Item *parent = world.FindItem(item.socket.container.item.parent_serial);
		    parent != nullptr &&
		    parent->attach_sequence != world.item_attach_sequence)
			AttachItem(server, protocol_version, world, *parent);

		/* then this item as container content */

		if (protocol_version < ProtocolVersion::V6) {
			/* convert to v5 packet */
			struct uo_packet_container_update p5;

			container_update_6_to_5(&p5, &item.socket.container);
			server.SendT(p5);
		} else {
			server.SendT(item.socket.container);
		}

		break;

	case UO::Command::Equip:
		server.SendT(item.socket.mobile);
		break;

	default:
		break;
	}

	if (item.packet_container_open.cmd == UO::Command::ContainerOpen) {
		if (protocol_version >= ProtocolVersion::V7) {
			struct uo_packet_container_open_7 p7 = {
				.base = item.packet_container_open,
				.zero = 0x00,
				.x7d = 0x7d,
			};

			server.SendT(p7);
		} else
			server.SendT(item.packet_container_open);
	}
}

void
SendWorld(UO::Server &server, ProtocolVersion protocol_version,
	  uint_least32_t supported_features_flags,
	  World &world)
{
	struct uo_packet_login_complete login_complete;

	/* 0x1b LoginConfirm */
	if (world.packet_start.cmd == UO::Command::Start)
		server.SendT(world.packet_start);

	/* 0xbf 0x08 MapChange */
	if (world.packet_map_change.length > 0) {
		assert(world.packet_map_change.cmd == UO::Command::Extended);
		assert(world.packet_map_change.length == sizeof(world.packet_map_change));
		assert(world.packet_map_change.extended_cmd == 0x0008);
		server.Send(VarLengthAsBytes(world.packet_map_change));
	}

	/* 0xbf 0x18 MapPatches */
	if (world.packet_map_patches.length > 0) {
		assert(world.packet_map_patches.cmd == UO::Command::Extended);
		assert(world.packet_map_patches.length == sizeof(world.packet_map_patches));
		assert(world.packet_map_patches.extended_cmd == 0x0018);
		server.Send(VarLengthAsBytes(world.packet_map_patches));
	}

	/* 0xbc SeasonChange */
	if (world.packet_season.cmd == UO::Command::Season)
		server.SendT(world.packet_season);

	/* 0xb9 SupportedFeatures */
	if (protocol_version >= ProtocolVersion::V6_0_14) {
		struct uo_packet_supported_features_6014 supported_features;
		supported_features.cmd = UO::Command::SupportedFeatures;
		supported_features.flags = supported_features_flags;
		server.SendT(supported_features);
	} else {
		struct uo_packet_supported_features supported_features;
		supported_features.cmd = UO::Command::SupportedFeatures;
		supported_features.flags = supported_features_flags;
		server.SendT(supported_features);
	}

	/* 0x4f GlobalLightLevel */
	if (world.packet_global_light_level.cmd == UO::Command::GlobalLightLevel)
		server.SendT(world.packet_global_light_level);

	/* 0x4e PersonalLightLevel */
	if (world.packet_personal_light_level.cmd == UO::Command::PersonalLightLevel)
		server.SendT(world.packet_personal_light_level);

	/* 0x20 MobileUpdate */
	if (world.packet_mobile_update.cmd == UO::Command::MobileUpdate)
		server.SendT(world.packet_mobile_update);

	/* WarMode */
	if (world.packet_war_mode.cmd == UO::Command::WarMode)
		server.SendT(world.packet_war_mode);

	/* mobiles */
	for (const auto &mobile : world.mobiles) {
		if (mobile.packet_mobile_incoming != nullptr)
			server.Send(mobile.packet_mobile_incoming);
		if (mobile.packet_mobile_status != nullptr)
			server.Send(mobile.packet_mobile_status);
	}

	/* items */
	++world.item_attach_sequence;
	for (auto &item : world.items)
		if (item.attach_sequence != world.item_attach_sequence)
			AttachItem(server, protocol_version, world, item);

	/* LoginComplete */
	login_complete.cmd = UO::Command::ReDrawAll;
	server.SendT(login_complete);
}
