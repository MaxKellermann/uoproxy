// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace UO {

class Decompression {
	int_least16_t treepos = 0;
	uint_least8_t bit = 8;
	uint_least8_t mask = 0;
	uint_least8_t value = 0;

public:
	/**
	 * Throws #SocketBufferFullError if the destination buffer is
	 * too small.
	 */
	std::size_t Decompress(unsigned char *dest, size_t dest_max_len,
			       std::span<const unsigned char> src);
};

/**
 * Throws #SocketBufferFullError if the destination buffer is
 * too small.
 */
std::size_t
Compress(unsigned char *dest, size_t dest_max_len,
	 std::span<const unsigned char> src);

} // namespace UO
