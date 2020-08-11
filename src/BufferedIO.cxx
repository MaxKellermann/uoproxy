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
#include "util/DynamicFifoBuffer.hxx"

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
read_to_buffer(int fd, DynamicFifoBuffer<uint8_t> &buffer, size_t length)
{
    assert(fd >= 0);

    auto w = buffer.Write();
    if (w.empty())
        return -2;

    ssize_t nbytes = recv(fd, w.data, std::min(w.size, length), MSG_DONTWAIT);
    if (nbytes > 0)
        buffer.Append(nbytes);

    return nbytes;
}

ssize_t
write_from_buffer(int fd, DynamicFifoBuffer<uint8_t> &buffer)
{
    auto r = buffer.Read();
    if (r.empty())
        return -2;

    ssize_t nbytes = send(fd, r.data, r.size, MSG_DONTWAIT);
    if (nbytes < 0 && errno != EAGAIN)
        return -1;

    if (nbytes <= 0)
        return r.size;

    buffer.Consume(nbytes);
    return (ssize_t)r.size - nbytes;
}
