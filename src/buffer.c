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
    b->position = 0;

    return b;
}

void buffer_delete(struct buffer *b) {
    b->max_length = 0;
    b->length = 0;
    free(b);
}

static inline void buffer_flush(struct buffer *b) {
    assert(b->position <= b->length);

    if (b->position == 0)
        return;

    b->length -= b->position;
    if (b->length > 0)
        memmove(b->data, b->data + b->position, b->length);

    b->position = 0;
}

void buffer_append(struct buffer *b, const void *data,
                   size_t nbytes) {
    assert(nbytes <= buffer_free(b));

    buffer_flush(b);

    memcpy(b->data + b->length, data, nbytes);
    b->length += nbytes;
}

void buffer_shift(struct buffer *b, size_t nbytes) {
    assert(b->position <= b->length);
    assert(nbytes <= b->length - b->position);

    b->position += nbytes;
}
