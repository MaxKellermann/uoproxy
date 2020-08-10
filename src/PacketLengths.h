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

#ifndef __PACKETS_H
#define __PACKETS_H

#include "PacketType.hxx"
#include "PacketStructs.hxx"
#include "pversion.h"

#ifdef WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PACKET_LENGTH_INVALID ((size_t)-1)

/**
 * Determines the length of the packet.  Returns '0' when the length
 * cannot be determined (yet) because max_length is too small.
 * Returns PACKET_LENGTH_INVALID when the packet contains invalid
 * data.  The length being bigger than max_length is not an error.
 */
size_t
get_packet_length(enum protocol_version protocol,
                  const void *q, size_t max_length);

#ifdef __cplusplus
}
#endif

#endif
