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

#ifndef __BUFFERED_IO_H
#define __BUFFERED_IO_H

#include <cstdint>

#include <sys/types.h>

template<typename T> class DynamicFifoBuffer;

/**
 * Appends data from a file to the buffer.
 *
 * @param fd the source file descriptor
 * @param buffer the destination buffer
 * @return -1 on error, -2 if the buffer is full, or the amount appended to the buffer
 */
ssize_t
read_to_buffer(int fd, DynamicFifoBuffer<uint8_t> &buffer, size_t length);

/**
 * Writes data from the buffer to the file.
 *
 * @param fd the destination file descriptor
 * @param buffer the source buffer
 * @return -1 on error, -2 if the buffer is empty, or the rest left in the buffer
 */
ssize_t
write_from_buffer(int fd, DynamicFifoBuffer<uint8_t> &buffer);

#endif
