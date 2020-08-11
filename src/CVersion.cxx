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

#include "CVersion.hxx"
#include "PacketType.hxx"
#include "VerifyPacket.hxx"

#include <string.h>

ClientVersion::~ClientVersion() noexcept
{
    delete seed;
}

static int
client_version_compare(const char *a, const char *b)
{
    char *a_endptr, *b_endptr;
    unsigned long a_int, b_int;

    assert(a != nullptr);
    assert(b != nullptr);

    while (1) {
        a_int = strtoul(a, &a_endptr, 10);
        b_int = strtoul(b, &b_endptr, 10);

        if (a_int < b_int)
            return -1;
        if (a_int > b_int)
            return 1;

        if (*a_endptr < *b_endptr)
            return -1;
        if (*a_endptr > *b_endptr)
            return 1;

        if (*a_endptr == 0)
            return 0;

        a = a_endptr + 1;
        b = b_endptr + 1;
    }
}

static enum protocol_version
determine_protocol_version(const char *version)
{
    if (client_version_compare(version, "7") >= 0)
        return PROTOCOL_7;
    else if (client_version_compare(version, "6.0.14") >= 0)
        return PROTOCOL_6_0_14;
    else if (client_version_compare(version, "6.0.5") >= 0)
        return PROTOCOL_6_0_5;
    else if (client_version_compare(version, "6.0.1.7") >= 0)
        return PROTOCOL_6;
    else if (client_version_compare(version, "1") >= 0)
        return PROTOCOL_5;
    else
        return PROTOCOL_UNKNOWN;
}

int
ClientVersion::Set(const struct uo_packet_client_version *_packet,
                   size_t length) noexcept
{
    if (!packet_verify_client_version(_packet, length))
        return 0;

    packet = {_packet, length};

    if (protocol == PROTOCOL_UNKNOWN)
        protocol = determine_protocol_version(_packet->version);
    return 1;
}

void
ClientVersion::Set(const char *version) noexcept
{
    size_t length = strlen(version);

    packet = VarStructPtr<struct uo_packet_client_version>(sizeof(*packet) + length);
    packet->cmd = PCK_ClientVersion;
    packet->length = packet.size();
    memcpy(packet->version, version, length + 1);

    protocol = determine_protocol_version(version);
}

void
ClientVersion::Seed(const struct uo_packet_seed &_seed) noexcept
{
    assert(seed == nullptr);
    assert(_seed.cmd == PCK_Seed);

    seed = new struct uo_packet_seed(_seed);

    /* this packet is only know to 6.0.5.0 clients, so we don't check
       the packet contents here */
    if (_seed.client_major >= 7)
        protocol = PROTOCOL_7;
    else
        protocol = PROTOCOL_6_0_5;
}
