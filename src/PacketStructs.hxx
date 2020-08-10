/*
 * uoproxy
 *
 * Copyright 2005-2020 Max Kellermann <max.kellermann@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#pragma once

#ifdef WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

#include <stdint.h>

/* 0x00 CreateCharacter */
struct uo_packet_create_character {
    uint8_t cmd;
    uint32_t unknown0, unknown1;
    uint8_t unknown2;
    char name[30];
    uint8_t unknown3[2];
    uint32_t flags;
    uint8_t unknown4[8];
    uint8_t profession;
    uint8_t unknown5[15];
    uint8_t female;
    uint8_t strength, dexterity, intelligence;
    uint8_t is1, vs1, is2, vs2, is3, vs3;
    uint16_t hue;
    uint16_t hair_val, hair_hue;
    uint16_t hair_valf, hair_huef;
    uint8_t unknown6;
    uint8_t city_index;
    uint32_t char_slot, client_ip;
    uint16_t shirt_hue, pants_hue;
} __attribute__ ((packed));

/* 0x02 Walk */
struct uo_packet_walk {
    uint8_t cmd;
    uint8_t direction, seq;
    uint32_t key;
} __attribute__ ((packed));

/* 0x03 TalkAscii */
struct uo_packet_talk_ascii {
    uint8_t cmd;
    uint16_t length;
    uint8_t type;
    uint16_t hue, font;
    char text[1];
} __attribute__ ((packed));

/* 0x06 Use */
struct uo_packet_use {
    uint8_t cmd;
    uint32_t serial;
} __attribute__ ((packed));

/* 0x07 LiftRequest */
struct uo_packet_lift_request {
    uint8_t cmd;
    uint32_t serial;
    uint16_t amount;
} __attribute__ ((packed));

/* 0x08 Drop */
struct uo_packet_drop {
    uint8_t cmd;
    uint32_t serial;
    uint16_t x, y;
    int8_t z;
    uint32_t dest_serial;
} __attribute__ ((packed));

/* 0x08 Drop (protocol v6) */
struct uo_packet_drop_6 {
    uint8_t cmd;
    uint32_t serial;
    uint16_t x, y;
    int8_t z;
    uint8_t unknown0;
    uint32_t dest_serial;
} __attribute__ ((packed));

/* 0x11 MobileStatus */
struct uo_packet_mobile_status {
    uint8_t cmd;
    uint16_t length;
    uint32_t serial;
    char name[30];
    uint16_t hits, hits_max;
    uint8_t rename;
    uint8_t flags;
    /* only if flags >= 0x03 */
    uint8_t female;
    uint16_t strength, dexterity, intelligence;
    uint16_t stamina, stamina_max, mana, mana_max;
    uint32_t total_gold;
    uint16_t physical_resistance_or_armor_rating;
    uint16_t weight;
    uint16_t stat_cap;
    uint8_t followers, followers_max;
    /* only if flags >= 0x04 */
    uint16_t fire_resistance, cold_resistance;
    uint16_t poison_resistance, energy_resistance;
    uint16_t luck;
    uint16_t damage_min, damage_max;
    uint32_t tithing_points;
} __attribute__ ((packed));

/* 0x12 Action */
struct uo_packet_action {
    uint8_t cmd;
    uint16_t length;
    uint8_t type;
    char command[1];
} __attribute__ ((packed));

/* 0x1a WorldItem */
struct uo_packet_world_item {
    uint8_t cmd;
    uint16_t length;
    uint32_t serial;
    uint16_t item_id;
    /* warning: the following properties may be optional */
    uint16_t amount;
    uint16_t x, y;
    uint8_t direction;
    int8_t z;
    uint16_t hue;
    uint8_t flags;
} __attribute__ ((packed));

/* 0x1b Start */
struct uo_packet_start {
    uint8_t cmd;
    uint32_t serial;
    uint32_t unknown0;
    uint16_t body;
    uint16_t x, y;
    int16_t z;
    uint8_t direction;
    uint8_t unknown1;
    uint32_t unknown2;
    uint16_t unknown3, unknown4;
    uint16_t map_width, map_height;
    uint8_t unknown5[6];
} __attribute__ ((packed));

/* 0x1c SpeakAscii */
struct uo_packet_speak_ascii {
    uint8_t cmd;
    uint16_t length;
    uint32_t serial;
    int16_t graphic;
    uint8_t type;
    uint16_t hue;
    uint16_t font;
    char name[30];
    char text[1];
} __attribute__ ((packed));

/* 0x1d Delete */
struct uo_packet_delete {
    uint8_t cmd;
    uint32_t serial;
} __attribute__ ((packed));

/* 0x20 MobileUpdate */
struct uo_packet_mobile_update {
    uint8_t cmd;
    uint32_t serial;
    uint16_t body;
    uint8_t unknown0;
    uint16_t hue;
    uint8_t flags;
    uint16_t x, y;
    uint16_t unknown1;
    uint8_t direction;
    int8_t z;
} __attribute__ ((packed));

/* 0x21 WalkCancel */
struct uo_packet_walk_cancel {
    uint8_t cmd;
    uint8_t seq;
    uint16_t x, y;
    uint8_t direction;
    int8_t z;
} __attribute__ ((packed));

/* 0x22 WalkAck */
struct uo_packet_walk_ack {
    uint8_t cmd;
    uint8_t seq, notoriety;
} __attribute__ ((packed));

/* 0x24 ContainerOpen */
struct uo_packet_container_open {
    uint8_t cmd;
    uint32_t serial;
    uint16_t gump_id;
} __attribute__ ((packed));

/* 0x24 ContainerOpen (protocol v7) */
struct uo_packet_container_open_7 {
    struct uo_packet_container_open base;

    uint8_t zero;
    uint8_t x7d;
} __attribute__ ((packed));

/* for 0x25 ContainerUpdate */
struct uo_packet_fragment_container_item {
    uint32_t serial;
    uint16_t item_id;
    uint8_t unknown0;
    uint16_t amount;
    uint16_t x, y;
    uint32_t parent_serial;
    uint16_t hue;
} __attribute__ ((packed));

/* for 0x25 ContainerUpdate (protocol v6) */
struct uo_packet_fragment_container_item_6 {
    uint32_t serial;
    uint16_t item_id;
    uint8_t unknown0;
    uint16_t amount;
    uint16_t x, y;
    uint8_t unknown1;
    uint32_t parent_serial;
    uint16_t hue;
} __attribute__ ((packed));

/* 0x25 ContainerUpdate */
struct uo_packet_container_update {
    uint8_t cmd;
    struct uo_packet_fragment_container_item item;
} __attribute__ ((packed));

/* 0x25 ContainerUpdate (protocol v6) */
struct uo_packet_container_update_6 {
    uint8_t cmd;
    struct uo_packet_fragment_container_item_6 item;
} __attribute__ ((packed));

/* 0x27 LiftReject */
struct uo_packet_lift_reject {
    uint8_t cmd;
    uint8_t reason;
} __attribute__ ((packed));

/* 0x2e Equip */
struct uo_packet_equip {
    uint8_t cmd;
    uint32_t serial;
    uint16_t item_id;
    uint8_t unknown0;
    uint8_t layer;
    uint32_t parent_serial;
    uint16_t hue;
} __attribute__ ((packed));

/* 0x3c ContainerContent */
struct uo_packet_container_content {
    uint8_t cmd;
    uint16_t length;
    uint16_t num;
    struct uo_packet_fragment_container_item items[1];
} __attribute__ ((packed));

/* 0x3c ContainerContent (protocol v6) */
struct uo_packet_container_content_6 {
    uint8_t cmd;
    uint16_t length;
    uint16_t num;
    struct uo_packet_fragment_container_item_6 items[1];
} __attribute__ ((packed));

/* 0x4f GlobalLightLevel */
struct uo_packet_global_light_level {
    uint8_t cmd;
    int8_t level;
} __attribute__ ((packed));

/* 0x4e PersonalLightLevel */
struct uo_packet_personal_light_level {
    uint8_t cmd;
    uint32_t serial;
    int8_t level;
} __attribute__ ((packed));

/* 0x53 PopupMessage */
struct uo_packet_popup_message {
    uint8_t cmd;
    uint8_t msg;
};

/* 0x55 ReDrawAll */
struct uo_packet_login_complete {
    uint8_t cmd;
} __attribute__ ((packed));

/* 0x5d PlayCharacter */
struct uo_packet_play_character {
    uint8_t cmd;
    uint32_t unknown0;
    char name[30];
    uint16_t unknown1;
    uint32_t flags;
    uint8_t unknown2[24];
    uint32_t slot, client_ip;
} __attribute__ ((packed));

/* 0x6c Target */
struct uo_packet_target {
    uint8_t cmd;
    uint8_t allow_ground;
    uint32_t target_id;
    uint8_t flags;
    uint32_t serial;
    uint16_t x, y, z;
    uint16_t graphic;
} __attribute__ ((packed));

/* 0x72 WarMode */
struct uo_packet_war_mode {
    uint8_t cmd;
    uint8_t war_mode;
    uint8_t unknown0[3];
} __attribute__ ((packed));

/* 0x73 Ping */
struct uo_packet_ping {
    uint8_t cmd;
    uint8_t id;
} __attribute__ ((packed));

/* 0x76 ZoneChange */
struct uo_packet_zone_change {
    uint8_t cmd;
    uint16_t x, y;
    int16_t z;
    uint8_t unknown0;
    uint16_t unknown1, unknown2;
    uint16_t map_width, map_height;
} __attribute__ ((packed));

/* 0x77 MobileMoving */
struct uo_packet_mobile_moving {
    uint8_t cmd;
    uint32_t serial;
    uint16_t body;
    uint16_t x, y;
    int8_t z;
    uint8_t direction;
    uint16_t hue;
    uint8_t flags;
    uint8_t notoriety;
} __attribute__ ((packed));

/* for 0x78 MobileIncoming */
struct uo_packet_fragment_mobile_item {
    uint32_t serial;
    uint16_t item_id;
    uint8_t layer;
    uint16_t hue; /* optional */
} __attribute__ ((packed));

/* 0x78 MobileIncoming */
struct uo_packet_mobile_incoming {
    uint8_t cmd;
    uint16_t length;
    uint32_t serial;
    uint16_t body;
    uint16_t x, y;
    int8_t z;
    uint8_t direction;
    uint16_t hue;
    uint8_t flags;
    uint8_t notoriety;
    struct uo_packet_fragment_mobile_item items[1];
    /* uint32_t zero; */
} __attribute__ ((packed));

/* 0x80 AccountLogin */
struct uo_packet_account_login {
    uint8_t cmd;
    char username[30];
    char password[30];
    uint8_t unknown1;
} __attribute__ ((packed));

/* 0x82 AccountLoginReject */
struct uo_packet_account_login_reject {
    uint8_t cmd;
    uint8_t reason;
} __attribute__ ((packed));

/* 0x8c Relay */
struct uo_packet_relay {
    uint8_t cmd;
    uint32_t ip;
    uint16_t port;
    uint32_t auth_id;
} __attribute__ ((packed));

/* 0x91 GameLogin */
struct uo_packet_game_login {
    uint8_t cmd;
    uint32_t auth_id;
    char username[30];
    char password[30];
} __attribute__ ((packed));

/* 0x97 WalkForce */
struct uo_packet_walk_force {
    uint8_t cmd;
    uint8_t direction;
} __attribute__ ((packed));

/* for 0xa0 PlayServer */
struct uo_packet_play_server {
    uint8_t cmd;
    uint16_t index;
} __attribute__ ((packed));

/* for 0xa8 ServerList */
struct uo_fragment_server_info {
    uint16_t index;
    char name[32];
    char full;
    uint8_t timezone;
    uint32_t address;
} __attribute__ ((packed));

/* 0xa8 ServerList */
struct uo_packet_server_list {
    uint8_t cmd;
    uint16_t length;
    uint8_t unknown_0x5d;
    uint16_t num_game_servers;
    struct uo_fragment_server_info game_servers[1];
} __attribute__ ((packed));

/* for 0xa9 CharList */
struct uo_fragment_character_info {
    char name[30];
    char password[30];
} __attribute__ ((packed));

/* 0xa9 CharList */
struct uo_packet_simple_character_list {
    uint8_t cmd;
    uint16_t length;
    uint8_t character_count;
    struct uo_fragment_character_info character_info[1];
    uint8_t city_count;
    uint32_t flags;
} __attribute__ ((packed));

/* 0xb9 SupportedFeatures */
struct uo_packet_supported_features {
    uint8_t cmd;
    uint16_t flags;
} __attribute__ ((packed));

/* 0xb9 SupportedFeatures (protocol 6.0.14.2) */
struct uo_packet_supported_features_6014 {
    uint8_t cmd;
    uint32_t flags;
} __attribute__ ((packed));

/* 0xad TalkUnicode */
struct uo_packet_talk_unicode {
    uint8_t cmd;
    uint16_t length;
    uint8_t type;
    uint16_t hue, font;
    char lang[4];
    uint16_t text[1];
} __attribute__ ((packed));

/* 0xb1 GumpResponse */
struct uo_packet_gump_response {
    uint8_t cmd;
    uint16_t length;
    uint32_t serial, type_id, button_id;
    uint8_t rest[];
} __attribute__ ((packed));

/* 0xbc Season */
struct uo_packet_season {
    uint8_t cmd;
    uint8_t season, play_sound;
} __attribute__ ((packed));

/* 0xbd ClientVersion */
struct uo_packet_client_version {
    uint8_t cmd;
    uint16_t length;
    char version[1];
} __attribute__ ((packed));

/* 0xbf 0x0004 CloseGump */
struct uo_packet_close_gump {
    uint8_t cmd;
    uint16_t length;
    uint16_t extended_cmd; /* 0x0004 */
    uint32_t type_id, button_id;
} __attribute__ ((packed));

/* 0xbf 0x0008 MapChange */
struct uo_packet_map_change {
    uint8_t cmd;
    uint16_t length;
    uint16_t extended_cmd; /* 0x0008 */
    uint8_t map_id;
} __attribute__ ((packed));

/* for 0xbf 0x0018 MapPatch */
struct uo_fragment_map_patch {
    uint32_t static_blocks;
    uint32_t land_blocks;
};

/* 0xbf 0x0018 MapPatch */
struct uo_packet_map_patches {
    uint8_t cmd;
    uint16_t length;
    uint16_t extended_cmd; /* 0x0018 */
    uint32_t map_count;
    struct uo_fragment_map_patch map_patches[4];
} __attribute__ ((packed));

/* 0xbf Extended */
struct uo_packet_extended {
    uint8_t cmd;
    uint16_t length;
    uint16_t extended_cmd;
} __attribute__ ((packed));

/* 0xd9 Hardware */
struct uo_packet_hardware {
    uint8_t cmd;
    uint8_t unknown0;
    uint32_t instance_id;
    uint32_t os_major, os_minor, os_revision;
    uint8_t cpu_manufacturer;
    uint32_t cpu_family, cpu_model, cpu_clock;
    uint8_t cpu_quantity;
    uint32_t physical_memory;
    uint32_t screen_width, screen_height, screen_depth;
    uint16_t dx_major, dx_minor;
    char vc_description[128];
    uint32_t vc_vendor_id, vc_device_id, vc_memory;
    uint8_t distribution;
    uint8_t clients_running, clients_installed, partial_installed;
    char language[8];
    uint8_t unknown1[64];
} __attribute__ ((packed));

/* 0xef Seed */
struct uo_packet_seed {
    uint8_t cmd;
    uint32_t seed;
    uint32_t client_major;
    uint32_t client_minor;
    uint32_t client_revision;
    uint32_t client_patch;
} __attribute__ ((packed));

/* 0xf0 ProtocolExtension */
struct uo_packet_protocol_extension {
    uint8_t cmd;
    uint16_t length;
    uint8_t extension;
} __attribute__ ((packed));

/* 0xf3 WorldItem7 */
struct uo_packet_world_item_7 {
    uint8_t cmd;
    uint16_t one;

    /**
     * 0x00 = tiledata; 0x01 = bodyvalue; 0x02 = multidata.
     */
    uint8_t type;

    uint32_t serial;
    uint16_t item_id;
    uint8_t direction;
    uint16_t amount;
    uint16_t amount2;
    uint16_t x, y;
    int8_t z;
    uint8_t light_level;
    uint16_t hue;
    uint8_t flags;
    uint8_t zero;
    uint8_t function;
} __attribute__ ((packed));

/* 0xF8 CreateCharacter7 */
struct uo_packet_create_character_7 {
    uint8_t cmd;
    uint32_t unknown0, unknown1;
    uint8_t unknown2;
    char name[30];
    uint8_t unknown3[2];
    uint32_t flags;
    uint8_t unknown4[8];
    uint8_t profession;
    uint8_t unknown5[15];
    uint8_t female;
    uint8_t strength, dexterity, intelligence;
    uint8_t is1, vs1, is2, vs2, is3, vs3, is4, vs4;
    uint16_t hue;
    uint16_t hair_val, hair_hue;
    uint16_t hair_valf, hair_huef;
    uint8_t unknown6;
    uint8_t city_index;
    uint32_t char_slot, client_ip;
    uint16_t shirt_hue, pants_hue;
} __attribute__ ((packed));
