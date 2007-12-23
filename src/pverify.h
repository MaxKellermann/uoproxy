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
        p->version[0] != 0 &&
        p->version[length - sizeof(*p)] == 0;
}

#endif
