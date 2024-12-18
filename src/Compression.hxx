// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <span>

#include <sys/types.h> /* for ssize_t */

struct uo_decompression {
    int bit, treepos, mask;
    unsigned char value;
};

void uo_decompression_init(struct uo_decompression *de);

ssize_t
uo_decompress(struct uo_decompression *de,
              unsigned char *dest, size_t dest_max_len,
              std::span<const unsigned char> src);

ssize_t
uo_compress(unsigned char *dest, size_t dest_max_len,
            std::span<const unsigned char> src);
