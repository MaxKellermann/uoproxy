// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <span>

/**
 * Manage an allocated variable-length struct.
 */
template<typename T>
class VarStructPtr {
	std::unique_ptr<std::byte[]> value;

	std::size_t the_size = 0;

public:
	VarStructPtr() = default;

	VarStructPtr(std::nullptr_t n):value(n) {}

	explicit VarStructPtr(std::size_t _size) noexcept
		:value(std::make_unique<std::byte[]>(_size)),
		 the_size(_size)
	{
		static_assert(alignof(T) == 1);
	}

	VarStructPtr(const T *src, std::size_t _size) noexcept
		:VarStructPtr(_size)
	{
		std::copy_n(reinterpret_cast<const std::byte *>(src),
			    _size, value.get());
	}

	VarStructPtr(VarStructPtr &&src) noexcept
		:value(std::move(src.value)),
		 the_size(src.the_size) {}

	VarStructPtr &operator=(VarStructPtr &&src) noexcept {
		value = std::move(src.value);
		the_size = src.the_size;
		return *this;
	}

	operator bool() const noexcept {
		return !!value;
	}

	bool operator==(std::nullptr_t n) const noexcept {
		return value == n;
	}

	bool operator!=(std::nullptr_t n) const noexcept {
		return value != n;
	}

	void reset() noexcept {
		value.reset();
		the_size = 0;
	}

	T *get() const noexcept {
		static_assert(alignof(T) == 1);
		return reinterpret_cast<T *>(value.get());
	}

	std::size_t size() const noexcept {
		return the_size;
	}

	T *operator->() const noexcept {
		return get();
	}

	T &operator*() const noexcept {
		return *get();
	}

	constexpr operator std::span<const std::byte>() const noexcept {
		return {value.get(), the_size};
	}
};
