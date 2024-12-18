// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "SocketUtil.hxx"

#ifdef __linux
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

int
socket_set_nodelay(int fd, int value)
{
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
                      &value, sizeof(value));
}

#endif
