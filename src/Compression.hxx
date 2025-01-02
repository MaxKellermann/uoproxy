// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <span>

#include <sys/types.h> /* for ssize_t */

namespace UO {

class Decompression {
	int bit = 8, treepos = 0, mask = 0;
	unsigned char value = 0;

public:
	ssize_t Decompress(unsigned char *dest, size_t dest_max_len,
			   std::span<const unsigned char> src) noexcept;
};

ssize_t
Compress(unsigned char *dest, size_t dest_max_len,
	 std::span<const unsigned char> src);

} // namespace UO
