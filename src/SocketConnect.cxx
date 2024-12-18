// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "SocketConnect.hxx"
#include "net/SocketAddress.hxx"
#include "net/SocketError.hxx"

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#endif

int
socket_connect(int domain, int type, int protocol,
               SocketAddress address)
{
    int fd = socket(domain, type, protocol);
    if (fd < 0)
        throw MakeSocketError("Failed to create socket");

    int ret = connect(fd, address.GetAddress(), address.GetSize());
    if (ret < 0) {
        const auto e = GetSocketError();
        close(fd);
        throw MakeSocketError(e, "Failed to connect");
    }

    return fd;
}
