// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "PVersion.hxx"

#include <cstddef>

inline constexpr std::size_t PACKET_LENGTH_INVALID(-1);

/**
 * Determines the length of the packet.  Returns '0' when the length
 * cannot be determined (yet) because max_length is too small.
 * Returns PACKET_LENGTH_INVALID when the packet contains invalid
 * data.  The length being bigger than max_length is not an error.
 */
std::size_t
get_packet_length(enum protocol_version protocol,
                  const void *q, std::size_t max_length);
