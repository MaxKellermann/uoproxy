// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

enum protocol_version {
	PROTOCOL_UNKNOWN = 0,
	PROTOCOL_5,
	PROTOCOL_6,
	PROTOCOL_6_0_5,
	PROTOCOL_6_0_14,
	PROTOCOL_7,
	PROTOCOL_COUNT
};

const char *
protocol_name(enum protocol_version protocol) noexcept;
