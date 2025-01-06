// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Server.hxx"
#include "uo/Command.hxx"
#include "uo/Packets.hxx"
#include "util/VarStructPtr.hxx"

#include <algorithm>

static void
write_fixed_string(std::span<char> dest, std::string_view src) noexcept
{
	if (src.size() >= dest.size()) {
		std::copy_n(src.begin(), dest.size(), dest.begin());
	} else {
		std::fill(std::copy(src.begin(), src.end(), dest.begin()),
			  dest.end(),
			  '\0');
	}
}

void
UO::Server::SpeakAscii(uint32_t serial,
		       int16_t graphic,
		       uint8_t type,
		       uint16_t hue, uint16_t font,
		       std::string_view name,
		       std::string_view text)
{
	struct uo_packet_speak_ascii *p;

	VarStructPtr<struct uo_packet_speak_ascii> ptr(sizeof(*p) + text.size());
	p = ptr.get();

	p->cmd = UO::Command::SpeakAscii;
	p->length = ptr.size();
	p->serial = serial;
	p->graphic = graphic;
	p->type = type;
	p->hue = hue;
	p->font = font;
	write_fixed_string(p->name, name);
	*std::copy(text.begin(), text.end(), p->text) = '\0';

	Send(ptr);
}

void
UO::Server::SpeakConsole(std::string_view text)
{
	SpeakAscii(0xffffffff, -1, 0x01, 0x35, 3, "uoproxy", text);
}
