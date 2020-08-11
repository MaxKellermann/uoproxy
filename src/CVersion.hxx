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

#include "PVersion.hxx"

#include <stddef.h>

struct ClientVersion {
    struct uo_packet_client_version *packet = nullptr;
    struct uo_packet_seed *seed = nullptr;
    size_t packet_length = 0;
    enum protocol_version protocol = PROTOCOL_UNKNOWN;

    ClientVersion() = default;
    ~ClientVersion() noexcept;

    ClientVersion(const ClientVersion &) = delete;
    ClientVersion &operator=(const ClientVersion &) = delete;

    bool IsDefined() const noexcept {
        return packet != nullptr;
    }

    int Set(const struct uo_packet_client_version *_packet,
            size_t length) noexcept;

    void Set(const char *version) noexcept;

    void Seed(const struct uo_packet_seed &_seed) noexcept;
};

#endif
