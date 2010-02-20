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

#include "socket_util.h"

#include <assert.h>
#include <fcntl.h>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#endif

#ifdef __linux
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

int
socket_set_nonblock(int fd, int value)
{
#ifdef WIN32
    u_long val = value;
    return ioctlsocket(fd, FIONBIO, &val);
#else
    int ret;

    assert(fd >= 0);

    ret = fcntl(fd, F_GETFL, 0);
    if (ret < 0)
        return ret;

    if (value)
        ret |= O_NONBLOCK;
    else
        ret &= ~O_NONBLOCK;

    return fcntl(fd, F_SETFL, ret);
#endif
}

#ifdef __linux

int
socket_set_nodelay(int fd, int value)
{
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
                      &value, sizeof(value));
}

#endif
