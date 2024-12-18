// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "BufferedIO.hxx"
#include "util/DynamicFifoBuffer.hxx"

#include <assert.h>
#include <errno.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0
#endif

ssize_t
read_to_buffer(int fd, DynamicFifoBuffer<std::byte> &buffer, size_t length)
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
write_from_buffer(int fd, DynamicFifoBuffer<std::byte> &buffer)
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
