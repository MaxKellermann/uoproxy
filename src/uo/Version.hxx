// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <cstdint>

enum class ProtocolVersion : uint_least8_t {
	UNKNOWN = 0,
	V5,
	V6,
	V6_0_5,
	V6_0_14,
	V7,
	COUNT
};

const char *
protocol_name(ProtocolVersion protocol) noexcept;
