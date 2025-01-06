// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "util/VarStructPtr.hxx"

#include <cstdint>
#include <string_view>

struct uo_packet_speak_ascii;
struct uo_packet_client_version;

namespace UO {

VarStructPtr<struct uo_packet_speak_ascii>
MakeSpeakAscii(uint32_t serial,
	       int16_t graphic,
	       uint8_t type,
	       uint16_t hue, uint16_t font,
	       std::string_view name,
	       std::string_view text) noexcept;

inline VarStructPtr<struct uo_packet_speak_ascii>
MakeSpeakConsole(std::string_view text) noexcept
{
	return MakeSpeakAscii(0xffffffff, -1, 0x01, 0x35, 3, "uoproxy", text);
}

VarStructPtr<struct uo_packet_client_version>
MakeClientVersion(std::string_view version) noexcept;


} // namespace UO
