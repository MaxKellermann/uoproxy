// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

/*
 * Collect all objects which have to be flushed, wait until
 * flush_end() is called an then flush them all at once.
 *
 * This is useful for buffering data being sent.
 */

#include "Flush.hxx"

#include <assert.h>

static IntrusiveList<PendingFlush> flush_pending;
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

    flush_pending.clear_and_dispose([](PendingFlush *flush){
        flush->DoFlush();
    });
}

void
PendingFlush::ScheduleFlush() noexcept
{
    if (!flush_postponed)
        DoFlush();
    else if (!is_linked) {
        is_linked = true;

        flush_pending.push_back(*this);
    }
}
