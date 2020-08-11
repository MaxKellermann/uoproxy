/*
 * uoproxy
 *
 * Copyright 2005-2020 Max Kellermann <max.kellermann@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

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
