// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <cstdint>
#include <span>

#include <sys/types.h> /* for ssize_t */

namespace UO {

class Decompression {
	int_least16_t treepos = 0;
	uint_least8_t bit = 8;
	uint_least8_t mask = 0;
	uint_least8_t value = 0;

public:
	ssize_t Decompress(unsigned char *dest, size_t dest_max_len,
			   std::span<const unsigned char> src) noexcept;
};

ssize_t
Compress(unsigned char *dest, size_t dest_max_len,
	 std::span<const unsigned char> src);

} // namespace UO
