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

/*
 * Collect all objects which have to be flushed, wait until
 * flush_end() is called an then flush them all at once.
 *
 * This is useful for buffering data being sent.
 */

#ifndef __UOPROXY_FLUSH_H
#define __UOPROXY_FLUSH_H

#include "list.h"

/**
 * An operation that has to be flushed.
 */
struct pending_flush {
    struct list_head siblings;
    void (*flush)(struct pending_flush *flush);
};

static inline void
flush_init(struct pending_flush *flush,
           void (*callback)(struct pending_flush *flush))
{
    flush->siblings.next = NULL;
    flush->flush = callback;
}

void
flush_begin(void);

void
flush_end(void);

void
flush_add(struct pending_flush *flush);

void
flush_del(struct pending_flush *flush);

#endif
