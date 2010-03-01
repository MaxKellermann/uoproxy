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

#include "flush.h"

#include <assert.h>

static struct list_head flush_pending;

void
flush_begin(void)
{
    assert(flush_pending.next == NULL);

    INIT_LIST_HEAD(&flush_pending);
}

void
flush_end(void)
{
    struct pending_flush *flush, *n;

    assert(flush_pending.next != NULL);

    list_for_each_entry_safe(flush, n, &flush_pending, siblings) {
        list_del(&flush->siblings);

        flush->siblings.next = NULL;
        flush->flush(flush);
    }

    flush_pending.next = NULL;
}

void
flush_add(struct pending_flush *flush)
{
    if (flush_pending.next == NULL)
        flush->flush(flush);
    else if (flush->siblings.next == NULL)
        list_add(&flush->siblings, &flush_pending);
}

void
flush_del(struct pending_flush *flush)
{
    if (flush->siblings.next != NULL) {
        list_del(&flush->siblings);
        flush->siblings.next = NULL;
    }
}
