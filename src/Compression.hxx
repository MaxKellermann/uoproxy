// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstddef>
#include <span>

namespace UO {

class Decompression {
	int_least16_t treepos = 0;
	uint_least8_t bit = 8;
	std::byte mask{};
	std::byte value{};

public:
	/**
	 * Throws #SocketBufferFullError if the destination buffer is
	 * too small.
	 */
	std::size_t Decompress(std::span<std::byte> dest, std::span<const std::byte> src);
};

/**
 * Throws #SocketBufferFullError if the destination buffer is
 * too small.
 */
std::size_t
Compress(std::span<std::byte> dest, std::span<const std::byte> src);

} // namespace UO
