// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "SocketConnect.hxx"

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
               const struct sockaddr *address, size_t address_length)
{
    int fd = socket(domain, type, protocol);
    if (fd < 0)
        return -errno;

    int ret = connect(fd, address, address_length);
    if (ret < 0) {
        int save_errno = errno;
        close(fd);
        return -save_errno;
    }

    return fd;
}
