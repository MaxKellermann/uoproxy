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

#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "ioutil.h"
#include "sockbuff.h"

int sock_buff_create(int fd, size_t input_max,
                     size_t output_max,
                     struct sock_buff **sbp) {
    struct sock_buff *sb;

    sb = (struct sock_buff*)malloc(sizeof(*sb));
    if (sb == NULL)
        return -ENOMEM;

    sb->fd = fd;
    sb->input = buffer_new(input_max);
    if (sb->input == NULL) {
        free(sb);
        return -ENOMEM;
    }

    sb->output = buffer_new(output_max);
    if (sb->output == NULL) {
        buffer_delete(sb->input);
        free(sb);
        return -ENOMEM;
    }

    *sbp = sb;

    return 0;
}

void sock_buff_dispose(struct sock_buff *sb) {
    if (sb->fd >= 0)
        close(sb->fd);

    buffer_delete(sb->input);
    free(sb);
}

int sock_buff_flush(struct sock_buff *sb) {
    const unsigned char *p;
    size_t length;
    ssize_t nbytes;

    p = buffer_peek(sb->output, &length);
    if (p == NULL)
        return 0;

    nbytes = write(sb->fd, p, length);
    if (nbytes < 0) {
        int save_errno = errno;
        close(sb->fd);
        sb->fd = -1;
        return -save_errno;
    }

    buffer_remove_head(sb->output, (size_t)nbytes);

    return 0;
}

void sock_buff_pre_select(struct sock_buff *sb,
                          struct selectx *sx) {
    if (sb->fd < 0)
        return;

    if (buffer_free(sb->input) > 0)
        selectx_add_read(sx, sb->fd);

    if (!buffer_empty(sb->output))
        selectx_add_write(sx, sb->fd);
}

int sock_buff_post_select(struct sock_buff *sb,
                          struct selectx *sx) {
    ssize_t nbytes;

    if (sb->fd < 0)
        return 0;

    if (FD_ISSET(sb->fd, &sx->readfds)) {
        nbytes = read(sb->fd, buffer_tail(sb->input),
                      buffer_free(sb->input));
        if (nbytes < 0) {
            int save_errno = errno;
            close(sb->fd);
            sb->fd = -1;
            return -save_errno;
        }

        if (nbytes == 0) {
            close(sb->fd);
            sb->fd = -1;
            return 0;
        }

        buffer_expand(sb->input, (size_t)nbytes);
    }

    if (FD_ISSET(sb->fd, &sx->writefds))
        return sock_buff_flush(sb);

    return 0;
}
