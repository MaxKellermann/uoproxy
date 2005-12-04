/*
 * uoproxy
 * $Id$
 *
 * (c) 2005 Max Kellermann <max@duempel.org>
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

enum uo_packet_type_t {
    PCK_Create = 0x00,
    PCK_Disconnect = 0x01,
    PCK_Walk = 0x02,
    PCK_Talk = 0x03,
    PCK_GodMode = 0x04,
    PCK_Attack = 0x05,
    PCK_DClick = 0x06,
    PCK_ItemPickupReq = 0x07,
    PCK_ItemDropReq = 0x08,
    PCK_Click = 0x09,
    PCK_EditItem = 0x0A,
    PCK_EditArea = 0x0B,
    PCK_EditTileData = 0x0C,
    PCK_EditNPC = 0x0D,
    PCK_EditTemplate = 0x0E,
    PCK_EditPaperdoll = 0x0F,
    PCK_EditHues = 0x10,
    PCK_Status = 0x11,
    PCK_DoAction = 0x12,
    PCK_ItemEquipReq = 0x13,
    PCK_GodLevitate = 0x14,
    PCK_Follow = 0x15,
    PCK_ScriptsGet = 0x16,
    PCK_ScriptsExe = 0x17,
    PCK_ScriptsAdd = 0x18,
    PCK_UnkSpeakNPC = 0x19,
    PCK_Put = 0x1a,
    PCK_Start = 0x1b,
    PCK_Speak = 0x1c,
    PCK_Delete = 0x1d,
    PCK_UnkAnimate = 0x1e,
    PCK_UnkExplode = 0x1f,
    PCK_MobileUpdate = 0x20,
    PCK_WalkCancel = 0x21,
    PCK_WalkAck = 0x22,
    PCK_DragAnim = 0x23,
    PCK_ContOpen = 0x24,
    PCK_ContAdd = 0x25,
    PCK_Kick = 0x26,
    PCK_DragCancel = 0x27,
    PCK_ClearSquare = 0x28,
    PCK_ObjectDropped = 0x29,
    PCK_UnkBlood = 0x2A,
    PCK_GodModeOK = 0x2B,
    PCK_DeathMenu = 0x2C,
    PCK_UnkHealth = 0x2D,
    PCK_ItemEquip = 0x2e,
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
    PCK_Content = 0x3c,
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
    PCK_LightPoint = 0x4e,
    PCK_Light = 0x4f,
    PCK_UnkBBHeader = 0x50,
    PCK_UnkBBMessage = 0x51,
    PCK_UnkPostMsg = 0x52,
    PCK_ClientState = 0x53,
    PCK_Sound = 0x54,
    PCK_ReDrawAll = 0x55,
    PCK_MapEdit = 0x56,
    PCK_UnkRegionsUpdate = 0x57,
    PCK_UnkRegionsNew = 0x58,
    PCK_UnkEffectNew = 0x59,
    PCK_EffectUpdate = 0x5a,
    PCK_Time = 0x5b,
    PCK_UnkVersionRestart = 0x5c,
    PCK_CharPlay = 0x5d,
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
    PCK_War = 0x72,
    PCK_Ping = 0x73,
    PCK_VendOpenBuy = 0x74,
    PCK_CharName = 0x75,
    PCK_ZoneChange = 0x76,
    PCK_CharMove = 0x77,
    PCK_Char = 0x78,
    PCK_UnkResourceGet = 0x79,
    PCK_UnkResourceData = 0x7a,
    PCK_UnkSequence = 0x7b,
    PCK_MenuItems = 0x7c,
    PCK_MenuChoice = 0x7d,
    PCK_GodGetView = 0x7e,
    PCK_GodViewInfo = 0x7f,
    PCK_AccountLogin = 0x80,
    PCK_CharList3 = 0x81,
    PCK_LogBad = 0x82,
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
    PCK_ServerSelect = 0xa0,
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
    PCK_TalkUNICODE = 0xad,
    PCK_SpeakUNICODE = 0xae,
    PCK_CharDeath = 0xaf,
    PCK_GumpDialog = 0xb0,
    PCK_GumpDialogRet = 0xb1,
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
    PCK_ExtData = 0xbf,
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
    PCK_AOSObjProp = 0xdc,
};

extern const size_t packet_lengths[0x100];

struct uo_packet_ping {
    unsigned char cmd;
    unsigned char id;
};

struct uo_packet_ping_ack {
    unsigned char cmd;
    unsigned char id;
};

struct uo_packet_account_login {
    unsigned char cmd;
    char username[30];
    char password[30];
    unsigned char unknown1;
} __attribute__ ((packed));

struct uo_packet_login_bad {
    unsigned char cmd;
    unsigned char reason;
} __attribute__ ((packed));

struct uo_fragment_server_info {
    u_int16_t index;
    char name[32];
    char full;
    unsigned char timezone;
    u_int32_t address;
} __attribute__ ((packed));

struct uo_packet_game_login {
    unsigned char cmd;
    u_int32_t auth_id;
    char username[30];
    char password[30];
} __attribute__ ((packed));

struct uo_packet_relay {
    unsigned char cmd;
    u_int32_t ip;
    u_int16_t port;
    u_int32_t auth_id;
} __attribute__ ((packed));

struct uo_packet_create_character {
    unsigned char cmd;
    u_int32_t unknown0, unknown1, unknown2;
    char name[30];
    u_int8_t unknown3[2];
    u_int32_t flags;
    u_int8_t unknown4[8];
    u_int8_t profession;
    u_int8_t unknown5[15];
    u_int8_t female;
    u_int8_t strength, dexterity, intelligence;
    u_int8_t is1, vs1, is2, vs2, is3, vs3;
    u_int16_t hue;
    u_int16_t hair_val, hair_hue;
    u_int16_t hair_valf, hair_huef;
    u_int8_t unknown6;
    u_int8_t city_index;
    u_int32_t char_slot, client_ip;
    u_int16_t shirt_hue, pants_hue;
} __attribute__ ((packed));

struct uo_packet_supported_features {
    unsigned char cmd;
    u_int16_t flags;
} __attribute__ ((packed));

struct uo_packet_start {
    unsigned char cmd;
    u_int32_t serial;
    u_int32_t unknown0;
    u_int16_t body, x, y, z;
    u_int8_t direction;
    u_int8_t unknown1;
    u_int32_t unknown2;
    u_int16_t unknown3, unknown4;
    u_int16_t map_width, map_height;
    unsigned char unknown5[6];
} __attribute__ ((packed));

struct uo_packet_zone_change {
    unsigned char cmd;
    u_int16_t x, y, z;
    u_int8_t unknown0;
    u_int16_t unknown1, unknown2;
    u_int16_t map_width, map_height;
} __attribute__ ((packed));

struct uo_packet_mobile_update {
    unsigned char cmd;
    u_int32_t serial;
    u_int16_t body;
    u_int8_t unknown0;
    u_int16_t hue;
    u_int8_t packet_flags;
    u_int16_t x, y;
    u_int16_t unknown1;
    u_int8_t direction;
    int8_t z;
} __attribute__ ((packed));

struct uo_packet_extended {
    unsigned char cmd;
    u_int16_t length;
    u_int16_t id;
} __attribute__ ((packed));

#endif
