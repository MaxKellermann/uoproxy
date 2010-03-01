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

#ifndef __UOPROXY_COMPRESSION_H
#define __UOPROXY_COMPRESSION_H

#include <sys/types.h> /* for ssize_t */

struct uo_decompression {
    int bit, treepos, mask;
    unsigned char value;
};

void uo_decompression_init(struct uo_decompression *de);

ssize_t uo_decompress(struct uo_decompression *de,
                      unsigned char *dest, size_t dest_max_len,
                      const unsigned char *src, size_t src_len);

ssize_t uo_compress(unsigned char *dest, size_t dest_max_len,
                    const unsigned char *src, size_t src_len);

#endif
