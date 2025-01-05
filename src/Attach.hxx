// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <cstdint>

enum class ProtocolVersion : uint_least8_t;

namespace UO {
class Server;
}

struct World;

void
SendWorld(UO::Server &server, ProtocolVersion protocol_version,
	  uint_least32_t supported_features_flags,
	  World &world);
