// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <cstdint>

namespace UO {

enum class Command : uint8_t {
	CreateCharacter = 0x00,
	Disconnect = 0x01,
	Walk = 0x02,
	TalkAscii = 0x03,
	GodMode = 0x04,
	Attack = 0x05,
	Use = 0x06,
	LiftRequest = 0x07,
	Drop = 0x08,
	Click = 0x09,
	EditItem = 0x0A,
	EditArea = 0x0B,
	EditTileData = 0x0C,
	EditNPC = 0x0D,
	EditTemplate = 0x0E,
	EditPaperdoll = 0x0F,
	EditHues = 0x10,
	MobileStatus = 0x11,
	Action = 0x12,
	ItemEquipReq = 0x13,
	GodLevitate = 0x14,
	Follow = 0x15,
	ScriptsGet = 0x16,
	ScriptsExe = 0x17,
	ScriptsAdd = 0x18,
	UnkSpeakNPC = 0x19,
	WorldItem = 0x1a,
	Start = 0x1b,
	SpeakAscii = 0x1c,
	Delete = 0x1d,
	UnkAnimate = 0x1e,
	UnkExplode = 0x1f,
	MobileUpdate = 0x20,
	WalkCancel = 0x21,
	WalkAck = 0x22,
	Resynchronize = 0x22,
	DragAnim = 0x23,
	ContainerOpen = 0x24,
	ContainerUpdate = 0x25,
	Kick = 0x26,
	LiftReject = 0x27,
	ClearSquare = 0x28,
	ObjectDropped = 0x29,
	UnkBlood = 0x2A,
	GodModeOK = 0x2B,
	DeathMenu = 0x2C,
	UnkHealth = 0x2D,
	Equip = 0x2e,
	Fight = 0x2f,
	UnkAttackOK = 0x30,
	UnkPeace = 0x31,
	UnkHackMove = 0x32,
	Pause = 0x33,
	CharStatReq = 0x34,
	EditResType = 0x35,
	EditResTiledata = 0x36,
	UnkMoveObject = 0x37,
	PathFind = 0x38,
	ChangeGroup = 0x39,
	Skill = 0x3a,
	VendorBuy = 0x3b,
	ContainerContent = 0x3c,
	UnkShip = 0x3d,
	UnkVersions = 0x3e,
	EditUpdateObj = 0x3f,
	EditUpdateTerrain = 0x40,
	EditUpdateTiledata = 0x41,
	EditUpdateArt = 0x42,
	EditUpdateAnim = 0x43,
	EditUpdateHues = 0x44,
	UnkVersionOK = 0x45,
	EditNewArt = 0x46,
	EditNewTerrain = 0x47,
	EditNewAnim = 0x48,
	EditNewHues = 0x49,
	UnkDestroyArt = 0x4a,
	UnkCheckVersion = 0x4b,
	_ScriptsNames = 0x4c,
	ScriptsFile = 0x4d,
	PersonalLightLevel = 0x4e,
	GlobalLightLevel = 0x4f,
	UnkBBHeader = 0x50,
	UnkBBMessage = 0x51,
	UnkPostMsg = 0x52,
	PopupMessage = 0x53,
	Sound = 0x54,
	ReDrawAll = 0x55,
	MapEdit = 0x56,
	UnkRegionsUpdate = 0x57,
	UnkRegionsNew = 0x58,
	UnkEffectNew = 0x59,
	EffectUpdate = 0x5a,
	Time = 0x5b,
	UnkVersionRestart = 0x5c,
	PlayCharacter = 0x5d,
	UnkServerList = 0x5e,
	UnkServerAdd = 0x5f,
	UnkServerDel = 0x60,
	UnkStaticDel = 0x61,
	UnkStaticMove = 0x62,
	UnkLoadArea = 0x63,
	UnkLoadAreaTry = 0x64,
	Weather = 0x65,
	BookPage = 0x66,
	UnkSimped = 0x67,
	UnkAddLSScript = 0x68,
	Options = 0x69,
	UnkFriendNotify = 0x6a,
	UnkUseKey = 0x6b,
	Target = 0x6c,
	PlayMusic = 0x6d,
	CharAction = 0x6e,
	SecureTrade = 0x6f,
	Effect = 0x70,
	BBoard = 0x71,
	WarMode = 0x72,
	Ping = 0x73,
	VendOpenBuy = 0x74,
	CharName = 0x75,
	ZoneChange = 0x76,
	MobileMoving = 0x77,
	MobileIncoming = 0x78,
	UnkResourceGet = 0x79,
	UnkResourceData = 0x7a,
	UnkSequence = 0x7b,
	MenuItems = 0x7c,
	MenuChoice = 0x7d,
	GodGetView = 0x7e,
	GodViewInfo = 0x7f,
	AccountLogin = 0x80,
	CharList3 = 0x81,
	AccountLoginReject = 0x82,
	CharDelete = 0x83,
	UnkPasswordChange = 0x84,
	DeleteBad = 0x85,
	CharList2 = 0x86,
	UnkResourceSend = 0x87,
	PaperDoll = 0x88,
	CorpEquip = 0x89,
	EditTrigger = 0x8A,
	GumpTextDisp = 0x8b,
	Relay = 0x8c,
	Unused8d = 0x8d,
	UnkCharMove = 0x8e,
	Unused8f = 0x8f,
	MapDisplay = 0x90,
	GameLogin = 0x91,
	EditMultiMul = 0x92,
	BookOpen = 0x93,
	EditSkillsMul = 0x94,
	DyeVat = 0x95,
	GodGameMon = 0x96,
	WalkForce = 0x97,
	UnkChangeName = 0x98,
	TargetMulti = 0x99,
	Prompt = 0x9a,
	HelpPage = 0x9b,
	GodAssist = 0x9c,
	GodSingle = 0x9d,
	VendOpenSell = 0x9e,
	VendorSell = 0x9f,
	PlayServer = 0xa0,
	StatChngStr = 0xa1,
	StatChngInt = 0xa2,
	StatChngDex = 0xa3,
	Spy = 0xa4,
	Web = 0xa5,
	Scroll = 0xa6,
	TipReq = 0xa7,
	ServerList = 0xa8,
	CharList = 0xa9,
	AttackOK = 0xaa,
	GumpInpVal = 0xab,
	GumpInpValRet = 0xac,
	TalkUnicode = 0xad,
	SpeakUnicode = 0xae,
	CharDeath = 0xaf,
	GumpDialog = 0xb0,
	GumpResponse = 0xb1,
	ChatReq = 0xb2,
	ChatText = 0xb3,
	TargetItems = 0xb4,
	Chat = 0xb5,
	ToolTipReq = 0xb6,
	ToolTip = 0xb7,
	CharProfile = 0xb8,
	SupportedFeatures = 0xb9,
	Arrow = 0xba,
	MailMsg = 0xbb,
	Season = 0xbc,
	ClientVersion = 0xbd,
	UnkVersionAssist = 0xbe,
	Extended = 0xbf,
	UnkHuedEffect = 0xc0,
	SpeakTable = 0xc1,
	UnkSpeakTableU = 0xc2,
	UnkGQEffect = 0xc3,
	UnkSemiVisible = 0xc4,
	UnkInvalidMap = 0xc5,
	UnkEnableInvalidMap = 0xc6,
	ParticleEffect = 0xc7,
	UnkUpdateRange = 0xc8,
	UnkTripTime = 0xc9,
	UnkUTripTime = 0xca,
	UnkGQCount = 0xcb,
	UnkTextIDandStr = 0xcc,
	AccountLogin2 = 0xcf,
	AOSTooltip = 0xd6,
	Hardware = 0xd9,
	AOSObjProp = 0xdc,
	DisplayGumpPacked = 0xdd,
	BugReport = 0xe0, // KR
	ClientType = 0xe1, // KR / SA
	NewCharacterAnimation = 0xe2, // KR
	EncryptionResponse = 0xe3, // KR
	EquipMacro = 0xec, // KR
	UnequipMacro = 0xed, // KR
	Seed = 0xef,
	ProtocolExtension = 0xf0,
	WorldItem7 = 0xf3,
	NewMapMessage = 0xf5,
	CreateCharacter7 = 0xf8,
	OpenUOStore = 0xfa,
	UpdateViewPublicHouseContents = 0xfb,
};

} // namespace UO
