// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "PVersion.hxx"

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

static_assert(std::size(protocol_names) == PROTOCOL_COUNT);

const char *
protocol_name(enum protocol_version protocol) noexcept
{
    assert((unsigned)protocol < PROTOCOL_COUNT);

    return protocol_names[protocol];
}
