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
 */

#include "socket_connect.h"

#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>

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
