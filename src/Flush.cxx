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

static IntrusiveList<struct pending_flush> flush_pending;
static bool flush_postponed = false;

void
flush_begin()
{
    assert(flush_pending.empty());
    assert(!flush_postponed);
    flush_postponed = true;
}

void
flush_end()
{
    assert(flush_postponed);
    flush_postponed = false;

    flush_pending.clear_and_dispose([](struct pending_flush *flush){
        flush->flush(flush);
    });
}

void
flush_add(struct pending_flush *flush)
{
    if (!flush_postponed)
        flush->flush(flush);
    else if (!flush->is_linked) {
        flush->is_linked = true;
        flush_pending.push_back(*flush);
    }
}
