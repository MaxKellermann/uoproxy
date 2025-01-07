// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <cstdint>
#include <string_view>

enum class BufferedResult;
class DefaultFifoBuffer;
enum class ProtocolVersion : uint_least8_t;

namespace UO {

class PacketHandler;

BufferedResult
ParsePackets(DefaultFifoBuffer &buffer, ProtocolVersion protocol_version,
	     std::string_view peer_name,
	     PacketHandler &handler);

} // namespace UO
