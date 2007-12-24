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

/*
 * Verify UO network packets.
 */

#ifndef __UOPROXY_PVERIFY_H
#define __UOPROXY_PVERIFY_H

#include "packets.h"

#include <assert.h>

static inline int
char_is_printable(char ch)
{
    return (signed char)ch >= 0x20;
}

static inline int
verify_printable_asciiz(const char *p, size_t length)
{
    size_t i;
    for (i = 0; i < length; ++i)
        if (!char_is_printable(p[i]))
            return 0;
    return p[length] == 0;
}

/**
 * Verifies that the specified packet really contains a string.
 */
static inline int
packet_verify_client_version(const struct uo_packet_client_version *p,
                             size_t length)
{
    assert(length >= 3);
    assert(p->cmd == PCK_ClientVersion);

    return length > sizeof(*p) &&
        verify_printable_asciiz(p->version, length - sizeof(*p));
}

/**
 * Verifies that the packet length is correct for the number of items.
 */
static inline int
packet_verify_container_content(const struct uo_packet_container_content *p,
                                size_t length)
{
    assert(length >= 3);
    assert(p->cmd == PCK_ContainerContent);

    return length >= sizeof(*p) - sizeof(p->items) &&
        length == sizeof(*p) - sizeof(p->items) + ntohs(p->num) * sizeof(p->items[0]);
}

#endif
