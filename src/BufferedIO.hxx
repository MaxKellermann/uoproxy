// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <cstddef>

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
read_to_buffer(int fd, DynamicFifoBuffer<std::byte> &buffer, size_t length);

/**
 * Writes data from the buffer to the file.
 *
 * @param fd the destination file descriptor
 * @param buffer the source buffer
 * @return -1 on error, -2 if the buffer is empty, or the rest left in the buffer
 */
ssize_t
write_from_buffer(int fd, DynamicFifoBuffer<std::byte> &buffer);
