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

#include "BufferedIO.hxx"
#include "FifoBuffer.hxx"

#include <assert.h>
#include <errno.h>

#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0
#endif

ssize_t
read_to_buffer(int fd, struct fifo_buffer *buffer, size_t length)
{
    void *dest;
    size_t max_length;
    ssize_t nbytes;

    assert(fd >= 0);
    assert(buffer != nullptr);

    dest = fifo_buffer_write(buffer, &max_length);
    if (dest == nullptr)
        return -2;

    if (length > max_length)
        length = max_length;

    nbytes = recv(fd, dest, length, MSG_DONTWAIT);
    if (nbytes > 0)
        fifo_buffer_append(buffer, (size_t)nbytes);

    return nbytes;
}

ssize_t
write_from_buffer(int fd, struct fifo_buffer *buffer)
{
    const void *data;
    size_t length;
    ssize_t nbytes;

    data = fifo_buffer_read(buffer, &length);
    if (data == nullptr)
        return -2;

    nbytes = send(fd, data, length, MSG_DONTWAIT);
    if (nbytes < 0 && errno != EAGAIN)
        return -1;

    if (nbytes <= 0)
        return length;

    fifo_buffer_consume(buffer, (size_t)nbytes);
    return (ssize_t)length - nbytes;
}
