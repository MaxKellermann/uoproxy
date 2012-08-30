/*
 * uoproxy
 *
 * (c) 2005-2010 Max Kellermann <max@duempel.org>
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

#ifndef __PACKETS_H
#define __PACKETS_H

#include "pversion.h"

#ifdef WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

#include <stdint.h>

enum uo_packet_type_t {
    PCK_CreateCharacter = 0x00,
    PCK_Disconnect = 0x01,
    PCK_Walk = 0x02,
    PCK_TalkAscii = 0x03,
    PCK_GodMode = 0x04,
    PCK_Attack = 0x05,
    PCK_Use = 0x06,
    PCK_LiftRequest = 0x07,
    PCK_Drop = 0x08,
    PCK_Click = 0x09,
    PCK_EditItem = 0x0A,
    PCK_EditArea = 0x0B,
    PCK_EditTileData = 0x0C,
    PCK_EditNPC = 0x0D,
    PCK_EditTemplate = 0x0E,
    PCK_EditPaperdoll = 0x0F,
    PCK_EditHues = 0x10,
    PCK_MobileStatus = 0x11,
    PCK_Action = 0x12,
    PCK_ItemEquipReq = 0x13,
    PCK_GodLevitate = 0x14,
    PCK_Follow = 0x15,
    PCK_ScriptsGet = 0x16,
    PCK_ScriptsExe = 0x17,
    PCK_ScriptsAdd = 0x18,
    PCK_UnkSpeakNPC = 0x19,
    PCK_WorldItem = 0x1a,
    PCK_Start = 0x1b,
    PCK_SpeakAscii = 0x1c,
    PCK_Delete = 0x1d,
    PCK_UnkAnimate = 0x1e,
    PCK_UnkExplode = 0x1f,
    PCK_MobileUpdate = 0x20,
    PCK_WalkCancel = 0x21,
    PCK_WalkAck = 0x22,
    PCK_Resynchronize = 0x22,
    PCK_DragAnim = 0x23,
    PCK_ContainerOpen = 0x24,
    PCK_ContainerUpdate = 0x25,
    PCK_Kick = 0x26,
    PCK_LiftReject = 0x27,
    PCK_ClearSquare = 0x28,
    PCK_ObjectDropped = 0x29,
    PCK_UnkBlood = 0x2A,
    PCK_GodModeOK = 0x2B,
    PCK_DeathMenu = 0x2C,
    PCK_UnkHealth = 0x2D,
    PCK_Equip = 0x2e,
    PCK_Fight = 0x2f,
    PCK_UnkAttackOK = 0x30,
    PCK_UnkPeace = 0x31,
    PCK_UnkHackMove = 0x32,
    PCK_Pause = 0x33,
    PCK_CharStatReq = 0x34,
    PCK_EditResType = 0x35,
    PCK_EditResTiledata = 0x36,
    PCK_UnkMoveObject = 0x37,
    PCK_PathFind = 0x38,
    PCK_ChangeGroup = 0x39,
    PCK_Skill = 0x3a,
    PCK_VendorBuy = 0x3b,
    PCK_ContainerContent = 0x3c,
    PCK_UnkShip = 0x3d,
    PCK_UnkVersions = 0x3e,
    PCK_EditUpdateObj = 0x3f,
    PCK_EditUpdateTerrain = 0x40,
    PCK_EditUpdateTiledata = 0x41,
    PCK_EditUpdateArt = 0x42,
    PCK_EditUpdateAnim = 0x43,
    PCK_EditUpdateHues = 0x44,
    PCK_UnkVersionOK = 0x45,
    PCK_EditNewArt = 0x46,
    PCK_EditNewTerrain = 0x47,
    PCK_EditNewAnim = 0x48,
    PCK_EditNewHues = 0x49,
    PCK_UnkDestroyArt = 0x4a,
    PCK_UnkCheckVersion = 0x4b,
    PCK__ScriptsNames = 0x4c,
    PCK_ScriptsFile = 0x4d,
    PCK_PersonalLightLevel = 0x4e,
    PCK_GlobalLightLevel = 0x4f,
    PCK_UnkBBHeader = 0x50,
    PCK_UnkBBMessage = 0x51,
    PCK_UnkPostMsg = 0x52,
    PCK_PopupMessage = 0x53,
    PCK_Sound = 0x54,
    PCK_ReDrawAll = 0x55,
    PCK_MapEdit = 0x56,
    PCK_UnkRegionsUpdate = 0x57,
    PCK_UnkRegionsNew = 0x58,
    PCK_UnkEffectNew = 0x59,
    PCK_EffectUpdate = 0x5a,
    PCK_Time = 0x5b,
    PCK_UnkVersionRestart = 0x5c,
    PCK_PlayCharacter = 0x5d,
    PCK_UnkServerList = 0x5e,
    PCK_UnkServerAdd = 0x5f,
    PCK_UnkServerDel = 0x60,
    PCK_UnkStaticDel = 0x61,
    PCK_UnkStaticMove = 0x62,
    PCK_UnkLoadArea = 0x63,
    PCK_UnkLoadAreaTry = 0x64,
    PCK_Weather = 0x65,
    PCK_BookPage = 0x66,
    PCK_UnkSimped = 0x67,
    PCK_UnkAddLSScript = 0x68,
    PCK_Options = 0x69,
    PCK_UnkFriendNotify = 0x6a,
    PCK_UnkUseKey = 0x6b,
    PCK_Target = 0x6c,
    PCK_PlayMusic = 0x6d,
    PCK_CharAction = 0x6e,
    PCK_SecureTrade = 0x6f,
    PCK_Effect = 0x70,
    PCK_BBoard = 0x71,
    PCK_WarMode = 0x72,
    PCK_Ping = 0x73,
    PCK_VendOpenBuy = 0x74,
    PCK_CharName = 0x75,
    PCK_ZoneChange = 0x76,
    PCK_MobileMoving = 0x77,
    PCK_MobileIncoming = 0x78,
    PCK_UnkResourceGet = 0x79,
    PCK_UnkResourceData = 0x7a,
    PCK_UnkSequence = 0x7b,
    PCK_MenuItems = 0x7c,
    PCK_MenuChoice = 0x7d,
    PCK_GodGetView = 0x7e,
    PCK_GodViewInfo = 0x7f,
    PCK_AccountLogin = 0x80,
    PCK_CharList3 = 0x81,
    PCK_AccountLoginReject = 0x82,
    PCK_CharDelete = 0x83,
    PCK_UnkPasswordChange = 0x84,
    PCK_DeleteBad = 0x85,
    PCK_CharList2 = 0x86,
    PCK_UnkResourceSend = 0x87,
    PCK_PaperDoll = 0x88,
    PCK_CorpEquip = 0x89,
    PCK_EditTrigger = 0x8A,
    PCK_GumpTextDisp = 0x8b,
    PCK_Relay = 0x8c,
    PCK_Unused8d = 0x8d,
    PCK_UnkCharMove = 0x8e,
    PCK_Unused8f = 0x8f,
    PCK_MapDisplay = 0x90,
    PCK_GameLogin = 0x91,
    PCK_EditMultiMul = 0x92,
    PCK_BookOpen = 0x93,
    PCK_EditSkillsMul = 0x94,
    PCK_DyeVat = 0x95,
    PCK_GodGameMon = 0x96,
    PCK_WalkForce = 0x97,
    PCK_UnkChangeName = 0x98,
    PCK_TargetMulti = 0x99,
    PCK_Prompt = 0x9a,
    PCK_HelpPage = 0x9b,
    PCK_GodAssist = 0x9c,
    PCK_GodSingle = 0x9d,
    PCK_VendOpenSell = 0x9e,
    PCK_VendorSell = 0x9f,
    PCK_PlayServer = 0xa0,
    PCK_StatChngStr = 0xa1,
    PCK_StatChngInt = 0xa2,
    PCK_StatChngDex = 0xa3,
    PCK_Spy = 0xa4,
    PCK_Web = 0xa5,
    PCK_Scroll = 0xa6,
    PCK_TipReq = 0xa7,
    PCK_ServerList = 0xa8,
    PCK_CharList = 0xa9,
    PCK_AttackOK = 0xaa,
    PCK_GumpInpVal = 0xab,
    PCK_GumpInpValRet = 0xac,
    PCK_TalkUnicode = 0xad,
    PCK_SpeakUnicode = 0xae,
    PCK_CharDeath = 0xaf,
    PCK_GumpDialog = 0xb0,
    PCK_GumpResponse = 0xb1,
    PCK_ChatReq = 0xb2,
    PCK_ChatText = 0xb3,
    PCK_TargetItems = 0xb4,
    PCK_Chat = 0xb5,
    PCK_ToolTipReq = 0xb6,
    PCK_ToolTip = 0xb7,
    PCK_CharProfile = 0xb8,
    PCK_SupportedFeatures = 0xb9,
    PCK_Arrow = 0xba,
    PCK_MailMsg = 0xbb,
    PCK_Season = 0xbc,
    PCK_ClientVersion = 0xbd,
    PCK_UnkVersionAssist = 0xbe,
    PCK_Extended = 0xbf,
    PCK_UnkHuedEffect = 0xc0,
    PCK_SpeakTable = 0xc1,
    PCK_UnkSpeakTableU = 0xc2,
    PCK_UnkGQEffect = 0xc3,
    PCK_UnkSemiVisible = 0xc4,
    PCK_UnkInvalidMap = 0xc5,
    PCK_UnkEnableInvalidMap = 0xc6,
    PCK_ParticleEffect = 0xc7,
    PCK_UnkUpdateRange = 0xc8,
    PCK_UnkTripTime = 0xc9,
    PCK_UnkUTripTime = 0xca,
    PCK_UnkGQCount = 0xcb,
    PCK_UnkTextIDandStr = 0xcc,
    PCK_AccountLogin2 = 0xcf,
    PCK_AOSTooltip = 0xd6,
    PCK_Hardware = 0xd9,
    PCK_AOSObjProp = 0xdc,
    PCK_DisplayGumpPacked = 0xdd,
    PCK_Seed = 0xef,
    PCK_ProtocolExtension = 0xf0,
    PCK_WorldItem7 = 0xf3,
};

extern const size_t packet_lengths[0x100];
extern const size_t packet_lengths_6[0x100];
extern const size_t packet_lengths_6014[0x100];

#define PACKET_LENGTH_INVALID ((size_t)-1)

/**
 * Determines the length of the packet.  Returns '0' when the length
 * cannot be determined (yet) because max_length is too small.
 * Returns PACKET_LENGTH_INVALID when the packet contains invalid
 * data.  The length being bigger than max_length is not an error.
 */
size_t
get_packet_length(enum protocol_version protocol,
                  const void *q, size_t max_length);

/* 0x00 CreateCharacter */
struct uo_packet_create_character {
    unsigned char cmd;
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
    unsigned char cmd;
    uint8_t direction, seq;
    uint32_t key;
} __attribute__ ((packed));

/* 0x03 TalkAscii */
struct uo_packet_talk_ascii {
    unsigned char cmd;
    uint16_t length;
    uint8_t type;
    uint16_t hue, font;
    char text[1];
} __attribute__ ((packed));

/* 0x06 Use */
struct uo_packet_use {
    unsigned char cmd;
    uint32_t serial;
} __attribute__ ((packed));

/* 0x07 LiftRequest */
struct uo_packet_lift_request {
    unsigned char cmd;
    uint32_t serial;
    uint16_t amount;
} __attribute__ ((packed));

/* 0x08 Drop */
struct uo_packet_drop {
    unsigned char cmd;
    uint32_t serial;
    uint16_t x, y;
    int8_t z;
    uint32_t dest_serial;
} __attribute__ ((packed));

/* 0x08 Drop (protocol v6) */
struct uo_packet_drop_6 {
    unsigned char cmd;
    uint32_t serial;
    uint16_t x, y;
    int8_t z;
    uint8_t unknown0;
    uint32_t dest_serial;
} __attribute__ ((packed));

/* 0x11 MobileStatus */
struct uo_packet_mobile_status {
    unsigned char cmd;
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
    unsigned char cmd;
    uint16_t length;
    uint8_t type;
    char command[1];
} __attribute__ ((packed));

/* 0x1a WorldItem */
struct uo_packet_world_item {
    unsigned char cmd;
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
    unsigned char cmd;
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
    unsigned char unknown5[6];
} __attribute__ ((packed));

/* 0x1c SpeakAscii */
struct uo_packet_speak_ascii {
    unsigned char cmd;
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
    unsigned char cmd;
    uint32_t serial;
} __attribute__ ((packed));

/* 0x20 MobileUpdate */
struct uo_packet_mobile_update {
    unsigned char cmd;
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
    unsigned char cmd;
    uint8_t seq;
    uint16_t x, y;
    uint8_t direction;
    int8_t z;
} __attribute__ ((packed));

/* 0x22 WalkAck */
struct uo_packet_walk_ack {
    unsigned char cmd;
    uint8_t seq, notoriety;
} __attribute__ ((packed));

/* 0x24 ContainerOpen */
struct uo_packet_container_open {
    unsigned char cmd;
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
    unsigned char cmd;
    struct uo_packet_fragment_container_item item;
} __attribute__ ((packed));

/* 0x25 ContainerUpdate (protocol v6) */
struct uo_packet_container_update_6 {
    unsigned char cmd;
    struct uo_packet_fragment_container_item_6 item;
} __attribute__ ((packed));

/* 0x27 LiftReject */
struct uo_packet_lift_reject {
    unsigned char cmd;
    uint8_t reason;
} __attribute__ ((packed));

/* 0x2e Equip */
struct uo_packet_equip {
    unsigned char cmd;
    uint32_t serial;
    uint16_t item_id;
    uint8_t unknown0;
    uint8_t layer;
    uint32_t parent_serial;
    uint16_t hue;
} __attribute__ ((packed));

/* 0x3c ContainerContent */
struct uo_packet_container_content {
    unsigned char cmd;
    uint16_t length;
    uint16_t num;
    struct uo_packet_fragment_container_item items[1];
} __attribute__ ((packed));

/* 0x3c ContainerContent (protocol v6) */
struct uo_packet_container_content_6 {
    unsigned char cmd;
    uint16_t length;
    uint16_t num;
    struct uo_packet_fragment_container_item_6 items[1];
} __attribute__ ((packed));

/* 0x4f GlobalLightLevel */
struct uo_packet_global_light_level {
    unsigned char cmd;
    int8_t level;
} __attribute__ ((packed));

/* 0x4e PersonalLightLevel */
struct uo_packet_personal_light_level {
    unsigned char cmd;
    uint32_t serial;
    int8_t level;
} __attribute__ ((packed));

/* 0x53 PopupMessage */
struct uo_packet_popup_message {
    unsigned char cmd;
    uint8_t msg;
};

/* 0x55 ReDrawAll */
struct uo_packet_login_complete {
    unsigned char cmd;
} __attribute__ ((packed));

/* 0x5d PlayCharacter */
struct uo_packet_play_character {
    unsigned char cmd;
    uint32_t unknown0;
    char name[30];
    uint16_t unknown1;
    uint32_t flags;
    uint8_t unknown2[24];
    uint32_t slot, client_ip;
} __attribute__ ((packed));

/* 0x6c Target */
struct uo_packet_target {
    unsigned char cmd;
    uint8_t allow_ground;
    uint32_t target_id;
    uint8_t flags;
    uint32_t serial;
    uint16_t x, y, z;
    uint16_t graphic;
} __attribute__ ((packed));

/* 0x72 WarMode */
struct uo_packet_war_mode {
    unsigned char cmd;
    uint8_t war_mode;
    uint8_t unknown0[3];
} __attribute__ ((packed));

/* 0x73 Ping */
struct uo_packet_ping {
    unsigned char cmd;
    unsigned char id;
} __attribute__ ((packed));

/* 0x76 ZoneChange */
struct uo_packet_zone_change {
    unsigned char cmd;
    uint16_t x, y;
    int16_t z;
    uint8_t unknown0;
    uint16_t unknown1, unknown2;
    uint16_t map_width, map_height;
} __attribute__ ((packed));

/* 0x77 MobileMoving */
struct uo_packet_mobile_moving {
    unsigned char cmd;
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
    unsigned char cmd;
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
    unsigned char cmd;
    char username[30];
    char password[30];
    unsigned char unknown1;
} __attribute__ ((packed));

/* 0x82 AccountLoginReject */
struct uo_packet_account_login_reject {
    unsigned char cmd;
    unsigned char reason;
} __attribute__ ((packed));

/* 0x8c Relay */
struct uo_packet_relay {
    unsigned char cmd;
    uint32_t ip;
    uint16_t port;
    uint32_t auth_id;
} __attribute__ ((packed));

/* 0x91 GameLogin */
struct uo_packet_game_login {
    unsigned char cmd;
    uint32_t auth_id;
    char username[30];
    char password[30];
} __attribute__ ((packed));

/* 0x97 WalkForce */
struct uo_packet_walk_force {
    unsigned char cmd;
    uint8_t direction;
} __attribute__ ((packed));

/* for 0xa0 PlayServer */
struct uo_packet_play_server {
    unsigned char cmd;
    uint16_t index;
} __attribute__ ((packed));

/* for 0xa8 ServerList */
struct uo_fragment_server_info {
    uint16_t index;
    char name[32];
    char full;
    unsigned char timezone;
    uint32_t address;
} __attribute__ ((packed));

/* 0xa8 ServerList */
struct uo_packet_server_list {
    unsigned char cmd;
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
    unsigned char cmd;
    uint16_t length;
    uint8_t character_count;
    struct uo_fragment_character_info character_info[1];
    uint8_t city_count;
    uint32_t flags;
} __attribute__ ((packed));

/* 0xb9 SupportedFeatures */
struct uo_packet_supported_features {
    unsigned char cmd;
    uint16_t flags;
} __attribute__ ((packed));

/* 0xb9 SupportedFeatures (protocol 6.0.14.2) */
struct uo_packet_supported_features_6014 {
    unsigned char cmd;
    uint32_t flags;
} __attribute__ ((packed));

/* 0xad TalkUnicode */
struct uo_packet_talk_unicode {
    unsigned char cmd;
    uint16_t length;
    uint8_t type;
    uint16_t hue, font;
    char lang[4];
    uint16_t text[1];
} __attribute__ ((packed));

/* 0xb1 GumpResponse */
struct uo_packet_gump_response {
    unsigned char cmd;
    uint16_t length;
    uint32_t serial, type_id, button_id;
    uint8_t rest[];
} __attribute__ ((packed));

/* 0xbc Season */
struct uo_packet_season {
    unsigned char cmd;
    uint8_t season, play_sound;
} __attribute__ ((packed));

/* 0xbd ClientVersion */
struct uo_packet_client_version {
    unsigned char cmd;
    uint16_t length;
    char version[1];
} __attribute__ ((packed));

/* 0xbf 0x0004 CloseGump */
struct uo_packet_close_gump {
    unsigned char cmd;
    uint16_t length;
    uint16_t extended_cmd; /* 0x0004 */
    uint32_t type_id, button_id;
} __attribute__ ((packed));

/* 0xbf 0x0008 MapChange */
struct uo_packet_map_change {
    unsigned char cmd;
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
    unsigned char cmd;
    uint16_t length;
    uint16_t extended_cmd; /* 0x0018 */
    uint32_t map_count;
    struct uo_fragment_map_patch map_patches[4];
} __attribute__ ((packed));

/* 0xbf Extended */
struct uo_packet_extended {
    unsigned char cmd;
    uint16_t length;
    uint16_t extended_cmd;
} __attribute__ ((packed));

/* 0xd9 Hardware */
struct uo_packet_hardware {
    unsigned char cmd;
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
    unsigned char cmd;
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

#endif
