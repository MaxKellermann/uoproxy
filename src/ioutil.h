/*
 * uoproxy
 * $Id$
 *
 * (c) 2005 Max Kellermann <max@duempel.org>
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

#ifndef __IOUTIL_H
#define __IOUTIL_H

#include <sys/select.h>

struct selectx {
    int maxfd;
    fd_set readfds;
    fd_set writefds;
    fd_set exceptfds;
};

static inline void selectx_clear(struct selectx *sx) {
    sx->maxfd = -1;
    FD_ZERO(&sx->readfds);
    FD_ZERO(&sx->writefds);
    FD_ZERO(&sx->exceptfds);
}

static inline void selectx_add_read(struct selectx *sx, int fd) {
    if (fd > sx->maxfd)
        sx->maxfd = fd;

    FD_SET(fd, &sx->readfds);
}

static inline void selectx_add_write(struct selectx *sx, int fd) {
    if (fd > sx->maxfd)
        sx->maxfd = fd;

    FD_SET(fd, &sx->writefds);
}

static inline int selectx(struct selectx *sx, struct timeval *timeout) {
    return select(sx->maxfd + 1, &sx->readfds,
                  &sx->writefds, &sx->exceptfds,
                  timeout);
}

#endif
