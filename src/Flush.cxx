/*
 * uoproxy
 *
 * Copyright 2005-2020 Max Kellermann <max.kellermann@gmail.com>
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

#include "Flush.hxx"

#include <assert.h>

static struct list_head flush_pending;

void
flush_begin()
{
    assert(flush_pending.next == nullptr);

    INIT_LIST_HEAD(&flush_pending);
}

void
flush_end()
{
    struct pending_flush *flush, *n;

    assert(flush_pending.next != nullptr);

    list_for_each_entry_safe(flush, n, &flush_pending, siblings) {
        list_del(&flush->siblings);

        flush->siblings.next = nullptr;
        flush->flush(flush);
    }

    flush_pending.next = nullptr;
}

void
flush_add(struct pending_flush *flush)
{
    if (flush_pending.next == nullptr)
        flush->flush(flush);
    else if (flush->siblings.next == nullptr)
        list_add(&flush->siblings, &flush_pending);
}

void
flush_del(struct pending_flush *flush)
{
    if (flush->siblings.next != nullptr) {
        list_del(&flush->siblings);
        flush->siblings.next = nullptr;
    }
}
