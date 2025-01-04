// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "WalkState.hxx"

#include <algorithm>
#include <cassert>

void
WalkState::pop_front() noexcept
{
	assert(queue_size > 0);

	std::move(queue.data() + 1, queue.data() + queue_size,
		  queue.data());

	--queue_size;

	if (queue_size == 0)
		server = nullptr;
}

WalkState::Item *
WalkState::FindSequence(uint8_t seq) noexcept
{
	unsigned i;

	for (i = 0; i < queue_size; i++)
		if (queue[i].seq == seq)
			return &queue[i];

	return nullptr;
}

void
WalkState::Remove(Item &item) noexcept
{
	assert(&item >= queue.data());
	assert(&item < queue.data() + queue_size);

	std::move(&item + 1, queue.data() + queue_size, &item);

	--queue_size;

	if (queue_size == 0)
		server = nullptr;
}
