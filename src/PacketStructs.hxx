// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "util/PackedBigEndian.hxx"

#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

#include <string.h>

namespace UO {

enum Command : uint8_t;

struct CredentialsFragment {
	char username[30];
	char password[30];

	bool operator==(const CredentialsFragment &other) const noexcept {
		return strncmp(username, other.username, sizeof(username)) == 0 &&
			strncmp(password, other.password, sizeof(password)) == 0;
	}

	bool operator!=(const CredentialsFragment &other) const noexcept {
		return !(*this == other);
	}
};

static_assert(alignof(CredentialsFragment) == 1);
static_assert(sizeof(CredentialsFragment) == 60);

} // namespace UO

/* 0x00 CreateCharacter */
struct uo_packet_create_character {
	UO::Command cmd;
	PackedBE32 unknown0, unknown1;
	uint8_t unknown2;
	char name[30];
	uint8_t unknown3[2];
	PackedBE32 flags;
	uint8_t unknown4[8];
	uint8_t profession;
	uint8_t unknown5[15];
	uint8_t female;
	uint8_t strength, dexterity, intelligence;
	uint8_t is1, vs1, is2, vs2, is3, vs3;
	PackedBE16 hue;
	PackedBE16 hair_val, hair_hue;
	PackedBE16 hair_valf, hair_huef;
	uint8_t unknown6;
	uint8_t city_index;
	PackedBE32 char_slot, client_ip;
	PackedBE16 shirt_hue, pants_hue;
};

static_assert(alignof(struct uo_packet_create_character) == 1);
static_assert(sizeof(uo_packet_create_character) == 0x68);

/* 0x02 Walk */
struct uo_packet_walk {
	UO::Command cmd;
	uint8_t direction, seq;
	PackedBE32 key;
};

static_assert(alignof(struct uo_packet_walk) == 1);
static_assert(sizeof(uo_packet_walk) == 0x7);

/* 0x03 TalkAscii */
struct uo_packet_talk_ascii {
	UO::Command cmd;
	PackedBE16 length;
	uint8_t type;
	PackedBE16 hue, font;
	char text[1];
};

static_assert(alignof(struct uo_packet_talk_ascii) == 1);

/* 0x06 Use */
struct uo_packet_use {
	UO::Command cmd;
	PackedBE32 serial;
};

static_assert(alignof(struct uo_packet_use) == 1);

/* 0x07 LiftRequest */
struct uo_packet_lift_request {
	UO::Command cmd;
	PackedBE32 serial;
	PackedBE16 amount;
};

static_assert(alignof(struct uo_packet_lift_request) == 1);

/* 0x08 Drop */
struct uo_packet_drop {
	UO::Command cmd;
	PackedBE32 serial;
	PackedBE16 x, y;
	int8_t z;
	PackedBE32 dest_serial;
};

static_assert(alignof(struct uo_packet_drop) == 1);

/* 0x08 Drop (protocol v6) */
struct uo_packet_drop_6 {
	UO::Command cmd;
	PackedBE32 serial;
	PackedBE16 x, y;
	int8_t z;
	uint8_t unknown0;
	PackedBE32 dest_serial;
};

static_assert(alignof(struct uo_packet_drop_6) == 1);

/* 0x11 MobileStatus */
struct uo_packet_mobile_status {
	UO::Command cmd;
	PackedBE16 length;
	PackedBE32 serial;
	char name[30];
	PackedBE16 hits, hits_max;
	uint8_t rename;
	uint8_t flags;
	/* only if flags >= 0x03 */
	uint8_t female;
	PackedBE16 strength, dexterity, intelligence;
	PackedBE16 stamina, stamina_max, mana, mana_max;
	PackedBE32 total_gold;
	PackedBE16 physical_resistance_or_armor_rating;
	PackedBE16 weight;
	PackedBE16 stat_cap;
	uint8_t followers, followers_max;
	/* only if flags >= 0x04 */
	PackedBE16 fire_resistance, cold_resistance;
	PackedBE16 poison_resistance, energy_resistance;
	PackedBE16 luck;
	PackedBE16 damage_min, damage_max;
	PackedBE32 tithing_points;
};

static_assert(alignof(struct uo_packet_mobile_status) == 1);

/* 0x12 Action */
struct uo_packet_action {
	UO::Command cmd;
	PackedBE16 length;
	uint8_t type;
	char command[1];
};

static_assert(alignof(struct uo_packet_action) == 1);

/* 0x1a WorldItem */
struct uo_packet_world_item {
	UO::Command cmd;
	PackedBE16 length;
	PackedBE32 serial;
	PackedBE16 item_id;
	/* warning: the following properties may be optional */
	PackedBE16 amount;
	PackedBE16 x, y;
	uint8_t direction;
	int8_t z;
	PackedBE16 hue;
	uint8_t flags;
};

static_assert(alignof(struct uo_packet_world_item) == 1);

/* 0x1b Start */
struct uo_packet_start {
	UO::Command cmd;
	PackedBE32 serial;
	PackedBE32 unknown0;
	PackedBE16 body;
	PackedBE16 x, y;
	PackedSignedBE16 z;
	uint8_t direction;
	uint8_t unknown1;
	PackedBE32 unknown2;
	PackedBE16 unknown3, unknown4;
	PackedBE16 map_width, map_height;
	uint8_t unknown5[6];
};

static_assert(alignof(struct uo_packet_start) == 1);

/* 0x1c SpeakAscii */
struct uo_packet_speak_ascii {
	UO::Command cmd;
	PackedBE16 length;
	PackedBE32 serial;
	PackedSignedBE16 graphic;
	uint8_t type;
	PackedBE16 hue;
	PackedBE16 font;
	char name[30];
	char text[1];
};

static_assert(alignof(struct uo_packet_speak_ascii) == 1);

/* 0x1d Delete */
struct uo_packet_delete {
	UO::Command cmd;
	PackedBE32 serial;
};

static_assert(alignof(struct uo_packet_delete) == 1);

/* 0x20 MobileUpdate */
struct uo_packet_mobile_update {
	UO::Command cmd;
	PackedBE32 serial;
	PackedBE16 body;
	uint8_t unknown0;
	PackedBE16 hue;
	uint8_t flags;
	PackedBE16 x, y;
	PackedBE16 unknown1;
	uint8_t direction;
	int8_t z;
};

static_assert(alignof(struct uo_packet_mobile_update) == 1);

/* 0x21 WalkCancel */
struct uo_packet_walk_cancel {
	UO::Command cmd;
	uint8_t seq;
	PackedBE16 x, y;
	uint8_t direction;
	int8_t z;
};

static_assert(alignof(struct uo_packet_walk_cancel) == 1);

/* 0x22 WalkAck */
struct uo_packet_walk_ack {
	UO::Command cmd;
	uint8_t seq, notoriety;
};

static_assert(alignof(struct uo_packet_walk_ack) == 1);

/* 0x24 ContainerOpen */
struct uo_packet_container_open {
	UO::Command cmd;
	PackedBE32 serial;
	PackedBE16 gump_id;
};

static_assert(alignof(struct uo_packet_container_open) == 1);

/* 0x24 ContainerOpen (protocol v7) */
struct uo_packet_container_open_7 {
	struct uo_packet_container_open base;

	uint8_t zero;
	uint8_t x7d;
};

static_assert(alignof(struct uo_packet_container_open_7) == 1);

/* for 0x25 ContainerUpdate */
struct uo_packet_fragment_container_item {
	PackedBE32 serial;
	PackedBE16 item_id;
	uint8_t unknown0;
	PackedBE16 amount;
	PackedBE16 x, y;
	PackedBE32 parent_serial;
	PackedBE16 hue;
};

static_assert(alignof(struct uo_packet_fragment_container_item) == 1);

/* for 0x25 ContainerUpdate (protocol v6) */
struct uo_packet_fragment_container_item_6 {
	PackedBE32 serial;
	PackedBE16 item_id;
	uint8_t unknown0;
	PackedBE16 amount;
	PackedBE16 x, y;
	uint8_t unknown1;
	PackedBE32 parent_serial;
	PackedBE16 hue;
};

static_assert(alignof(struct uo_packet_fragment_container_item_6) == 1);

/* 0x25 ContainerUpdate */
struct uo_packet_container_update {
	UO::Command cmd;
	struct uo_packet_fragment_container_item item;
};

static_assert(alignof(struct uo_packet_container_update) == 1);

/* 0x25 ContainerUpdate (protocol v6) */
struct uo_packet_container_update_6 {
	UO::Command cmd;
	struct uo_packet_fragment_container_item_6 item;
};

static_assert(alignof(struct uo_packet_container_update_6) == 1);

/* 0x27 LiftReject */
struct uo_packet_lift_reject {
	UO::Command cmd;
	uint8_t reason;
};

static_assert(alignof(struct uo_packet_lift_reject) == 1);

/* 0x2e Equip */
struct uo_packet_equip {
	UO::Command cmd;
	PackedBE32 serial;
	PackedBE16 item_id;
	uint8_t unknown0;
	uint8_t layer;
	PackedBE32 parent_serial;
	PackedBE16 hue;
};

static_assert(alignof(struct uo_packet_equip) == 1);

/* 0x3c ContainerContent */
struct uo_packet_container_content {
	UO::Command cmd;
	PackedBE16 length;
	PackedBE16 num;
	struct uo_packet_fragment_container_item items[1];
};

static_assert(alignof(struct uo_packet_container_content) == 1);

/* 0x3c ContainerContent (protocol v6) */
struct uo_packet_container_content_6 {
	UO::Command cmd;
	PackedBE16 length;
	PackedBE16 num;
	struct uo_packet_fragment_container_item_6 items[1];
};

static_assert(alignof(struct uo_packet_container_content_6) == 1);

/* 0x4f GlobalLightLevel */
struct uo_packet_global_light_level {
	UO::Command cmd;
	int8_t level;
};

static_assert(alignof(struct uo_packet_global_light_level) == 1);

/* 0x4e PersonalLightLevel */
struct uo_packet_personal_light_level {
	UO::Command cmd;
	PackedBE32 serial;
	int8_t level;
};

static_assert(alignof(struct uo_packet_personal_light_level) == 1);

/* 0x53 PopupMessage */
struct uo_packet_popup_message {
	UO::Command cmd;
	uint8_t msg;
};

static_assert(alignof(struct uo_packet_popup_message) == 1);

/* 0x55 ReDrawAll */
struct uo_packet_login_complete {
	UO::Command cmd;
};

static_assert(alignof(struct uo_packet_login_complete) == 1);

/* 0x5d PlayCharacter */
struct uo_packet_play_character {
	UO::Command cmd;
	PackedBE32 unknown0;
	char name[30];
	PackedBE16 unknown1;
	PackedBE32 flags;
	uint8_t unknown2[24];
	PackedBE32 slot, client_ip;
};

static_assert(alignof(struct uo_packet_play_character) == 1);

/* 0x6c Target */
struct uo_packet_target {
	UO::Command cmd;
	uint8_t allow_ground;
	PackedBE32 target_id;
	uint8_t flags;
	PackedBE32 serial;
	PackedBE16 x, y, z;
	PackedBE16 graphic;
};

static_assert(alignof(struct uo_packet_target) == 1);

/* 0x72 WarMode */
struct uo_packet_war_mode {
	UO::Command cmd;
	uint8_t war_mode;
	uint8_t unknown0[3];
};

static_assert(alignof(struct uo_packet_war_mode) == 1);

/* 0x73 Ping */
struct uo_packet_ping {
	UO::Command cmd;
	uint8_t id;
};

static_assert(alignof(struct uo_packet_ping) == 1);

/* 0x76 ZoneChange */
struct uo_packet_zone_change {
	UO::Command cmd;
	PackedBE16 x, y;
	PackedSignedBE16 z;
	uint8_t unknown0;
	PackedBE16 unknown1, unknown2;
	PackedBE16 map_width, map_height;
};

static_assert(alignof(struct uo_packet_zone_change) == 1);

/* 0x77 MobileMoving */
struct uo_packet_mobile_moving {
	UO::Command cmd;
	PackedBE32 serial;
	PackedBE16 body;
	PackedBE16 x, y;
	int8_t z;
	uint8_t direction;
	PackedBE16 hue;
	uint8_t flags;
	uint8_t notoriety;
};

static_assert(alignof(struct uo_packet_mobile_moving) == 1);

/* for 0x78 MobileIncoming */
struct uo_packet_fragment_mobile_item {
	PackedBE32 serial;
	PackedBE16 item_id;
	uint8_t layer;
	PackedBE16 hue; /* optional */
};

static_assert(alignof(struct uo_packet_fragment_mobile_item) == 1);

/* 0x78 MobileIncoming */
struct uo_packet_mobile_incoming {
	UO::Command cmd;
	PackedBE16 length;
	PackedBE32 serial;
	PackedBE16 body;
	PackedBE16 x, y;
	int8_t z;
	uint8_t direction;
	PackedBE16 hue;
	uint8_t flags;
	uint8_t notoriety;
	struct uo_packet_fragment_mobile_item items[1];
	/* PackedBE32 zero; */
};

static_assert(alignof(struct uo_packet_mobile_incoming) == 1);

/* 0x80 AccountLogin */
struct uo_packet_account_login {
	UO::Command cmd;
	UO::CredentialsFragment credentials;
	uint8_t unknown1;
};

static_assert(alignof(struct uo_packet_account_login) == 1);
static_assert(sizeof(struct uo_packet_account_login) == 62);

/* 0x82 AccountLoginReject */
struct uo_packet_account_login_reject {
	UO::Command cmd;
	uint8_t reason;
};

static_assert(alignof(struct uo_packet_account_login_reject) == 1);

/* 0x8c Relay */
struct uo_packet_relay {
	UO::Command cmd;
	PackedBE32 ip;
	PackedBE16 port;
	PackedBE32 auth_id;
};

static_assert(alignof(struct uo_packet_relay) == 1);

/* 0x91 GameLogin */
struct uo_packet_game_login {
	UO::Command cmd;
	PackedBE32 auth_id;
	UO::CredentialsFragment credentials;
};

static_assert(alignof(struct uo_packet_game_login) == 1);
static_assert(sizeof(struct uo_packet_game_login) == 65);

/* 0x97 WalkForce */
struct uo_packet_walk_force {
	UO::Command cmd;
	uint8_t direction;
};

static_assert(alignof(struct uo_packet_walk_force) == 1);

/* for 0xa0 PlayServer */
struct uo_packet_play_server {
	UO::Command cmd;
	PackedBE16 index;
};

static_assert(alignof(struct uo_packet_play_server) == 1);

/* for 0xa8 ServerList */
struct uo_fragment_server_info {
	PackedBE16 index;
	char name[32];
	char full;
	uint8_t timezone;
	PackedBE32 address;
};

static_assert(alignof(struct uo_fragment_server_info) == 1);

/* 0xa8 ServerList */
struct uo_packet_server_list {
	UO::Command cmd;
	PackedBE16 length;
	uint8_t unknown_0x5d;
	PackedBE16 num_game_servers;
	struct uo_fragment_server_info game_servers[1];
};

static_assert(alignof(struct uo_packet_server_list) == 1);

/* for 0xa9 CharList */
struct uo_fragment_character_info {
	char name[30];
	char password[30];
};

static_assert(alignof(struct uo_fragment_character_info) == 1);

/* 0xa9 CharList */
struct uo_packet_simple_character_list {
	UO::Command cmd;
	PackedBE16 length;
	uint8_t character_count;
	struct uo_fragment_character_info character_info[1];
	uint8_t city_count;
	PackedBE32 flags;

	bool IsValidCharacterIndex(unsigned i) const noexcept {
		return i < character_count &&
			character_info[i].name[0] != 0;
	}
};

static_assert(alignof(struct uo_packet_simple_character_list) == 1);

/* 0xb9 SupportedFeatures */
struct uo_packet_supported_features {
	UO::Command cmd;
	PackedBE16 flags;
};

static_assert(alignof(struct uo_packet_supported_features) == 1);

/* 0xb9 SupportedFeatures (protocol 6.0.14.2) */
struct uo_packet_supported_features_6014 {
	UO::Command cmd;
	PackedBE32 flags;
};

static_assert(alignof(struct uo_packet_supported_features_6014) == 1);

/* 0xad TalkUnicode */
struct uo_packet_talk_unicode {
	UO::Command cmd;
	PackedBE16 length;
	uint8_t type;
	PackedBE16 hue, font;
	char lang[4];
	PackedBE16 text[1];
};

static_assert(alignof(struct uo_packet_talk_unicode) == 1);

/* 0xb1 GumpResponse */
struct uo_packet_gump_response {
	UO::Command cmd;
	PackedBE16 length;
	PackedBE32 serial, type_id, button_id;
	// uint8_t rest[];
};

static_assert(alignof(struct uo_packet_gump_response) == 1);

/* 0xbc Season */
struct uo_packet_season {
	UO::Command cmd;
	uint8_t season, play_sound;
};

static_assert(alignof(struct uo_packet_season) == 1);

/* 0xbd ClientVersion */
struct uo_packet_client_version {
	UO::Command cmd;
	PackedBE16 length;
	char version[1];
};

static_assert(alignof(struct uo_packet_client_version) == 1);

/* 0xbf 0x0004 CloseGump */
struct uo_packet_close_gump {
	UO::Command cmd;
	PackedBE16 length;
	PackedBE16 extended_cmd; /* 0x0004 */
	PackedBE32 type_id, button_id;
};

static_assert(alignof(struct uo_packet_close_gump) == 1);

/* 0xbf 0x0008 MapChange */
struct uo_packet_map_change {
	UO::Command cmd;
	PackedBE16 length;
	PackedBE16 extended_cmd; /* 0x0008 */
	uint8_t map_id;
};

static_assert(alignof(struct uo_packet_map_change) == 1);

/* for 0xbf 0x0018 MapPatch */
struct uo_fragment_map_patch {
	PackedBE32 static_blocks;
	PackedBE32 land_blocks;
};

static_assert(alignof(struct uo_fragment_map_patch) == 1);

/* 0xbf 0x0018 MapPatch */
struct uo_packet_map_patches {
	UO::Command cmd;
	PackedBE16 length;
	PackedBE16 extended_cmd; /* 0x0018 */
	PackedBE32 map_count;
	struct uo_fragment_map_patch map_patches[4];
};

static_assert(alignof(struct uo_packet_map_patches) == 1);

/* 0xbf Extended */
struct uo_packet_extended {
	UO::Command cmd;
	PackedBE16 length;
	PackedBE16 extended_cmd;
};

static_assert(alignof(struct uo_packet_extended) == 1);

/* 0xd9 Hardware */
struct uo_packet_hardware {
	UO::Command cmd;
	uint8_t unknown0;
	PackedBE32 instance_id;
	PackedBE32 os_major, os_minor, os_revision;
	uint8_t cpu_manufacturer;
	PackedBE32 cpu_family, cpu_model, cpu_clock;
	uint8_t cpu_quantity;
	PackedBE32 physical_memory;
	PackedBE32 screen_width, screen_height, screen_depth;
	PackedBE16 dx_major, dx_minor;
	char vc_description[128];
	PackedBE32 vc_vendor_id, vc_device_id, vc_memory;
	uint8_t distribution;
	uint8_t clients_running, clients_installed, partial_installed;
	char language[8];
	uint8_t unknown1[64];
};

static_assert(alignof(struct uo_packet_hardware) == 1);

/* 0xef Seed */
struct uo_packet_seed {
	UO::Command cmd;
	PackedBE32 seed;
	PackedBE32 client_major;
	PackedBE32 client_minor;
	PackedBE32 client_revision;
	PackedBE32 client_patch;
};

static_assert(alignof(struct uo_packet_seed) == 1);

/* 0xf0 ProtocolExtension */
struct uo_packet_protocol_extension {
	UO::Command cmd;
	PackedBE16 length;
	uint8_t extension;
};

static_assert(alignof(struct uo_packet_protocol_extension) == 1);

/* 0xf3 WorldItem7 */
struct uo_packet_world_item_7 {
	UO::Command cmd;
	PackedBE16 one;

	/**
	 * 0x00 = tiledata; 0x01 = bodyvalue; 0x02 = multidata.
	 */
	uint8_t type;

	PackedBE32 serial;
	PackedBE16 item_id;
	uint8_t direction;
	PackedBE16 amount;
	PackedBE16 amount2;
	PackedBE16 x, y;
	int8_t z;
	uint8_t light_level;
	PackedBE16 hue;
	uint8_t flags;
	uint8_t zero;
	uint8_t function;
};

static_assert(alignof(struct uo_packet_world_item_7) == 1);

/* 0xF8 CreateCharacter7 */
struct uo_packet_create_character_7 {
	UO::Command cmd;
	PackedBE32 unknown0, unknown1;
	uint8_t unknown2;
	char name[30];
	uint8_t unknown3[2];
	PackedBE32 flags;
	uint8_t unknown4[8];
	uint8_t profession;
	uint8_t unknown5[15];
	uint8_t female;
	uint8_t strength, dexterity, intelligence;
	uint8_t is1, vs1, is2, vs2, is3, vs3, is4, vs4;
	PackedBE16 hue;
	PackedBE16 hair_val, hair_hue;
	PackedBE16 hair_valf, hair_huef;
	uint8_t unknown6;
	uint8_t city_index;
	PackedBE32 char_slot, client_ip;
	PackedBE16 shirt_hue, pants_hue;
};

static_assert(alignof(struct uo_packet_create_character_7) == 1);

template<typename T>
concept AnyPacket = std::has_unique_object_representations_v<T> && requires (const T &t) {
	{ t.cmd } -> std::same_as<const UO::Command &>;
};

template<typename T>
concept VarLengthPacket = AnyPacket<T> && requires (const T &t) {
	{ t.length } -> std::same_as<const PackedBE16 &>;
};

constexpr std::span<const std::byte>
VarLengthAsBytes(const VarLengthPacket auto &packet) noexcept
{
	return { reinterpret_cast<const std::byte *>(&packet), packet.length };
}
