// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "MakePacket.hxx"
#include "Command.hxx"
#include "Packets.hxx"

#include <algorithm> // for std::copy()

namespace UO {

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

VarStructPtr<struct uo_packet_speak_ascii>
MakeSpeakAscii(uint32_t serial,
	       int16_t graphic,
	       uint8_t type,
	       uint16_t hue, uint16_t font,
	       std::string_view name,
	       std::string_view text) noexcept
{
	struct uo_packet_speak_ascii *p;
	VarStructPtr<struct uo_packet_speak_ascii> ptr{sizeof(*p) + text.size()};
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

	return ptr;
}

VarStructPtr<struct uo_packet_client_version>
MakeClientVersion(std::string_view version) noexcept
{
	struct uo_packet_client_version *p;
	VarStructPtr<struct uo_packet_client_version> ptr{sizeof(*p) + version.size()};
	p = ptr.get();

	p->cmd = UO::Command::ClientVersion;
	p->length = ptr.size();
	*std::copy(version.begin(), version.end(), p->version) = '\0';

	return ptr;
}

} // namespace UO
