/*
 * Copyright 2020 Max Kellermann <max.kellermann@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>

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
};
