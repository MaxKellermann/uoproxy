// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <cassert>
#include <cstddef>

template<bool enable> class OptionalCounter;

template<>
class OptionalCounter<false>
{
public:
	constexpr void reset() noexcept {}
	constexpr auto &operator++() noexcept { return *this; }
	constexpr auto &operator--() noexcept { return *this; }
};

template<>
class OptionalCounter<true>
{
	std::size_t value = 0;

public:
	constexpr operator std::size_t() const noexcept {
		return value;
	}

	constexpr void reset() noexcept {
		value = 0;
	}

	constexpr auto &operator++() noexcept {
		++value;
		return *this;
	}

	constexpr auto &operator--() noexcept {
		assert(value > 0);

		--value;
		return *this;
	}
};
