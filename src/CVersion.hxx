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

#ifndef __UOPROXY_CVERSION_H
#define __UOPROXY_CVERSION_H

#include "pversion.h"

#include <stddef.h>

struct client_version {
    struct uo_packet_client_version *packet = nullptr;
    struct uo_packet_seed *seed = nullptr;
    size_t packet_length = 0;
    enum protocol_version protocol = PROTOCOL_UNKNOWN;

    client_version() = default;
    ~client_version() noexcept;

    client_version(const client_version &) = delete;
    client_version &operator=(const client_version &) = delete;
};

static inline int
client_version_defined(const struct client_version *cv)
{
    return cv->packet != nullptr;
}

int
client_version_copy(struct client_version *cv,
                    const struct uo_packet_client_version *packet,
                    size_t length);

int
client_version_set(struct client_version *cv,
                   const char *version);

int
client_version_seed(struct client_version *cv,
                    const struct uo_packet_seed *seed);

#endif
