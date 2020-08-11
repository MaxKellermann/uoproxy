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

#ifndef __UOPROXY_FLUSH_H
#define __UOPROXY_FLUSH_H

#include "util/IntrusiveList.hxx"

/**
 * An operation that has to be flushed.
 */
struct PendingFlush final : IntrusiveListHook {
    bool is_linked = false;

    void (*flush)(PendingFlush *flush);

    explicit PendingFlush(void (*_callback)(PendingFlush *)) noexcept
        :flush(_callback) {}

    ~PendingFlush() noexcept {
        Cancel();
    }

    PendingFlush(const PendingFlush &) = delete;
    PendingFlush &operator=(const PendingFlush &) = delete;

    void Cancel() noexcept {
        if (is_linked) {
            is_linked = false;
            unlink();
        }
    }
};

void
flush_begin();

void
flush_end();

void
flush_add(PendingFlush *flush);

#endif
