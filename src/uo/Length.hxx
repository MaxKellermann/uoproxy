// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "Version.hxx"

#include <cstddef>
#include <span>

static constexpr std::size_t PACKET_LENGTH_INVALID = ~std::size_t{0};

/**
 * Determines the length of the packet.  Returns '0' when the length
 * cannot be determined (yet) because max_length is too small.
 * Returns PACKET_LENGTH_INVALID when the packet contains invalid
 * data.  The length being bigger than max_length is not an error.
 */
[[gnu::pure]]
std::size_t
GetPacketLength(std::span<const std::byte> src,
		enum protocol_version protocol) noexcept;
