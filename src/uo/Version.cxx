// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Version.hxx"

#include <cassert>
#include <iterator> // for std::size()

static constexpr const char *protocol_names[] = {
	"unknown",
	"5 or older",
	"6.0.1.7",
	"6.0.5.0",
	"6.0.14.2",
	"7",
};

static_assert(std::size(protocol_names) == static_cast<std::size_t>(ProtocolVersion::COUNT));

const char *
protocol_name(ProtocolVersion protocol) noexcept
{
	const auto i = static_cast<std::size_t>(protocol);
	assert(i < static_cast<std::size_t>(ProtocolVersion::COUNT));

	return protocol_names[i];
}
