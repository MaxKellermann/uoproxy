// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "SocketEvent.hxx"
#include "Loop.hxx"
#include "event/Features.h"

#include <cassert>
#include <utility>

#ifdef USE_EPOLL
#include <cerrno>
#endif

void
SocketEvent::Open(SocketDescriptor _fd) noexcept
{
	assert(_fd.IsDefined());
	assert(!fd.IsDefined());
	assert(GetScheduledFlags() == 0);

	fd = _fd;
}

void
SocketEvent::Close() noexcept
{
	if (!fd.IsDefined())
		return;

	/* closing the socket automatically unregisters it from epoll,
	   so we can omit the epoll_ctl(EPOLL_CTL_DEL) call and save
	   one system call */
	if (std::exchange(scheduled_flags, 0) != 0) {
		/* TODO: due to a Linux kernel bug which calls
		   eventpoll_release() delayed in a workqueue (via
		   fput()), the close() system call does not
		   unregister the file in epoll immediately, and
		   spurious events can still fire; this optimization
		   needs to be disabled until the kernel bug has been
		   fixed */
#define HAVE_THREADED_EVENT_LOOP

#ifdef HAVE_THREADED_EVENT_LOOP
		/* can't use this optimization in multi-threaded
		   programs, because all file descriptors get
		   duplicated in forked processes, leaving them
		   registered in epoll, which could cause the parent
		   to crash */
		loop.RemoveFD(fd.Get(), *this);
#else
		loop.AbandonFD(*this);
#endif
	}
	fd.Close();
}

void
SocketEvent::Abandon() noexcept
{
	if (std::exchange(scheduled_flags, 0) != 0)
		loop.AbandonFD(*this);

	fd = SocketDescriptor::Undefined();
}

bool
SocketEvent::Schedule(unsigned flags) noexcept
{
	if (flags != 0)
		flags |= IMPLICIT_FLAGS;

	if (flags == GetScheduledFlags())
		return true;

	assert(IsDefined());

	bool success;
	if (scheduled_flags == 0)
		success = loop.AddFD(fd.Get(), flags, *this);
	else if (flags == 0)
		success = loop.RemoveFD(fd.Get(), *this);
	else
		success = loop.ModifyFD(fd.Get(), flags, *this);

	if (success)
		scheduled_flags = flags;
#ifdef USE_EPOLL
	else if (errno == EBADF || errno == ENOENT)
		/* the socket was probably closed by somebody else
		   (EBADF) or a new file descriptor with the same
		   number was created but not registered already
		   (ENOENT) - we can assume that there are no
		   scheduled events */
		/* note that when this happens, we're actually lucky
		   that it has failed - imagine another thread may
		   meanwhile have created something on the same file
		   descriptor number, and has registered it; the
		   epoll_ctl() call above would then have succeeded,
		   but broke the other thread's epoll registration */
		scheduled_flags = 0;
#endif

	return success;
}

void
SocketEvent::Dispatch() noexcept
{
	const unsigned flags = std::exchange(ready_flags, 0) &
		GetScheduledFlags();

	if (flags != 0)
		callback(flags);
}
