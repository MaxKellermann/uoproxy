// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "util/IntrusiveList.hxx"

/**
 * An operation that has to be flushed.
 */
class PendingFlush : public IntrusiveListHook<> {
	bool is_linked = false;

public:
	PendingFlush() = default;

	~PendingFlush() noexcept {
		CancelFlush();
	}

	PendingFlush(const PendingFlush &) = delete;
	PendingFlush &operator=(const PendingFlush &) = delete;

	virtual void DoFlush() noexcept = 0;

protected:
	void ScheduleFlush() noexcept;

	void CancelFlush() noexcept {
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

class ScopeLockFlush {
public:
	ScopeLockFlush() noexcept {
		flush_begin();
	}

	~ScopeLockFlush() noexcept {
		flush_end();
	}

	ScopeLockFlush(const ScopeLockFlush &) = delete;
	ScopeLockFlush &operator=(const ScopeLockFlush &) = delete;
};
