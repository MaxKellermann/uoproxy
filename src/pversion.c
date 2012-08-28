/*
 * uoproxy
 *
 * (c) 2005-2010 Max Kellermann <max@duempel.org>
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

#include "pversion.h"

#include <assert.h>

static const char *protocol_names[PROTOCOL_COUNT] = {
    [PROTOCOL_UNKNOWN] = "unknown",
    [PROTOCOL_5] = "5 or older",
    [PROTOCOL_6] = "6.0.1.7",
    [PROTOCOL_6_0_5] = "6.0.5.0",
    [PROTOCOL_6_0_14] = "6.0.14.2",
    [PROTOCOL_7] = "7",
};

const char *
protocol_name(enum protocol_version protocol)
{
    assert((unsigned)protocol < PROTOCOL_COUNT);

    return protocol_names[protocol];
}
