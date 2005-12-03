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
#include <stdlib.h>
#include <string.h>

#include "buffer.h"

struct buffer *buffer_new(size_t max_length) {
    struct buffer *b;

    assert(max_length > 0);

    b = (struct buffer*)malloc(sizeof(*b) + max_length - 1);
    if (b == NULL)
        return NULL;

    b->max_length = max_length;
    b->length = 0;

    return b;
}

void buffer_delete(struct buffer *b) {
    b->max_length = 0;
    b->length = 0;
    free(b);
}

void buffer_append(struct buffer *b, const unsigned char *data,
                   size_t nbytes) {
    assert(nbytes <= buffer_free(b));

    memcpy(b->data + b->length, data, nbytes);
    b->length += nbytes;
}

void buffer_remove_head(struct buffer *b,
                        size_t nbytes) {
    assert(nbytes <= b->length);

    b->length -= nbytes;
    if (b->length > 0)
        memmove(b->data, b->data + nbytes, b->length);
}
