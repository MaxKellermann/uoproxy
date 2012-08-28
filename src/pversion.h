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

#ifndef __UOPROXY_PVERSION_H
#define __UOPROXY_PVERSION_H

enum protocol_version {
    PROTOCOL_UNKNOWN = 0,
    PROTOCOL_5,
    PROTOCOL_6,
    PROTOCOL_6_0_5,
    PROTOCOL_6_0_14,
    PROTOCOL_7,
    PROTOCOL_COUNT
};

const char *
protocol_name(enum protocol_version protocol);

#endif
