/*
 * uoproxy
 *
 * (c) 2005-2007 Max Kellermann <max@duempel.org>
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

#include "cversion.h"
#include "packets.h"
#include "pverify.h"

#include <stdlib.h>
#include <string.h>

void
client_version_free(struct client_version *cv)
{
    if (cv->packet != NULL)
        free(cv->packet);
}

static int
client_version_compare(const char *a, const char *b)
{
    char *a_endptr, *b_endptr;
    unsigned long a_int, b_int;

    assert(a != NULL);
    assert(b != NULL);

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
    if (client_version_compare(version, "6") >= 0)
        return PROTOCOL_6;
    else if (client_version_compare(version, "1") >= 0)
        return PROTOCOL_5;
    else
        return PROTOCOL_UNKNOWN;
}

int
client_version_copy(struct client_version *cv,
                    const struct uo_packet_client_version *packet,
                    size_t length)
{
    if (!packet_verify_client_version(packet, length))
        return 0;

    cv->packet = malloc(length);
    if (cv->packet == NULL)
        return -1;

    cv->packet_length = length;

    memcpy(cv->packet, packet, length);

    cv->protocol = determine_protocol_version(packet->version);
    return 1;
}

int
client_version_set(struct client_version *cv,
                   const char *version)
{
    size_t length = strlen(version);

    cv->packet = malloc(sizeof(*cv->packet) + length);
    if (cv->packet == NULL)
        return -1;

    cv->packet->cmd = PCK_ClientVersion;
    cv->packet->length = htons(sizeof(*cv->packet) + length);
    memcpy(cv->packet->version, version, length + 1);

    cv->protocol = determine_protocol_version(version);
    return 1;
}
