// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Server.hxx"
#include "uo/MakePacket.hxx"
#include "util/VarStructPtr.hxx"

void
UO::Server::SpeakAscii(uint32_t serial,
		       int16_t graphic,
		       uint8_t type,
		       uint16_t hue, uint16_t font,
		       std::string_view name,
		       std::string_view text)
{
	Send(MakeSpeakAscii(serial, graphic, type, hue, font, name, text));
}

void
UO::Server::SpeakConsole(std::string_view text)
{
	Send(MakeSpeakConsole(text));
}
