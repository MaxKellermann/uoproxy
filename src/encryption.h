/*
 * uoproxy
 *
 * (c) 2005-2012 Max Kellermann <max@duempel.org>
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

#ifndef UOPROXY_ENCRYPTION_H
#define UOPROXY_ENCRYPTION_H

#include <stddef.h>

struct encryption;

struct encryption *
encryption_new(void);

void
encryption_free(struct encryption *e);

/**
 * @return encrypted data (may be the original #data pointer if the
 * connection is not encrypted), or NULL if more data is necessary
 */
const void *
encryption_from_client(struct encryption *e,
                       const void *data, size_t length);

#endif
