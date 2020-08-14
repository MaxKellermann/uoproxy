// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <cstdint>

#if defined(__i386__) || defined(__x86_64__) || defined(__ARMEL__)
/* well-known little-endian */
#  define IS_LITTLE_ENDIAN true
#  define IS_BIG_ENDIAN false
#elif defined(__MIPSEB__)
/* well-known big-endian */
#  define IS_LITTLE_ENDIAN false
#  define IS_BIG_ENDIAN true
#elif defined(__APPLE__) || defined(__NetBSD__)
/* compile-time check for MacOS */
#  include <machine/endian.h>
#  if BYTE_ORDER == LITTLE_ENDIAN
#    define IS_LITTLE_ENDIAN true
#    define IS_BIG_ENDIAN false
#  else
#    define IS_LITTLE_ENDIAN false
#    define IS_BIG_ENDIAN true
#  endif
#elif defined(__BYTE_ORDER__)
/* GCC-specific macros */
#  if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#    define IS_LITTLE_ENDIAN true
#    define IS_BIG_ENDIAN false
#  else
#    define IS_LITTLE_ENDIAN false
#    define IS_BIG_ENDIAN true
#  endif
#else
/* generic compile-time check */
#  include <endian.h>
#  if __BYTE_ORDER == __LITTLE_ENDIAN
#    define IS_LITTLE_ENDIAN true
#    define IS_BIG_ENDIAN false
#  else
#    define IS_LITTLE_ENDIAN false
#    define IS_BIG_ENDIAN true
#  endif
#endif

constexpr bool
IsLittleEndian() noexcept
{
	return IS_LITTLE_ENDIAN;
}

constexpr bool
IsBigEndian() noexcept
{
	return IS_BIG_ENDIAN;
}

constexpr uint16_t
GenericByteSwap16(uint16_t value) noexcept
{
	return (value >> 8) | (value << 8);
}

constexpr uint32_t
GenericByteSwap32(uint32_t value) noexcept
{
	return (value >> 24) | ((value >> 8) & 0x0000ff00) |
		((value << 8) & 0x00ff0000) | (value << 24);
}

constexpr uint64_t
GenericByteSwap64(uint64_t value) noexcept
{
	return uint64_t(GenericByteSwap32(uint32_t(value >> 32)))
		| (uint64_t(GenericByteSwap32(value)) << 32);
}

constexpr uint16_t
ByteSwap16(uint16_t value) noexcept
{
#ifdef __GNUC__
	return __builtin_bswap16(value);
#else
	return GenericByteSwap16(value);
#endif
}

constexpr uint32_t
ByteSwap32(uint32_t value) noexcept
{
#ifdef __GNUC__
	return __builtin_bswap32(value);
#else
	return GenericByteSwap32(value);
#endif
}

constexpr uint64_t
ByteSwap64(uint64_t value) noexcept
{
#ifdef __GNUC__
	return __builtin_bswap64(value);
#else
	return GenericByteSwap64(value);
#endif
}

/**
 * A packed big-endian 16 bit integer.
 */
class PackedBE16 {
	uint8_t hi, lo;

public:
	PackedBE16() = default;

	constexpr PackedBE16(uint16_t src) noexcept
		:hi(uint8_t(src >> 8)),
		 lo(uint8_t(src)) {}

	/**
	 * Construct an instance from an integer which is already
	 * big-endian.
	 */
	static constexpr auto FromBE(uint16_t src) noexcept {
		union {
			uint16_t in;
			PackedBE16 out;
		} u{src};
		return u.out;
	}

	constexpr operator uint16_t() const {
		return (uint16_t(hi) << 8) | uint16_t(lo);
	}

	/**
	 * Reads the raw, big-endian value.
	 */
	constexpr uint16_t raw() const noexcept {
		uint16_t x = *this;
		if (IsLittleEndian())
			x = ByteSwap16(x);
		return x;
	}
};

static_assert(sizeof(PackedBE16) == sizeof(uint16_t), "Wrong size");
static_assert(alignof(PackedBE16) == 1, "Wrong alignment");

/**
 * A packed big-endian signed 16 bit integer.
 */
class PackedSignedBE16 {
	PackedBE16 u;

public:
	PackedSignedBE16() = default;

	constexpr PackedSignedBE16(int16_t src) noexcept
		:u(uint16_t(src)) {}

	constexpr operator int16_t() const {
		return (int16_t)(uint16_t)u;
	}
};

static_assert(sizeof(PackedSignedBE16) == sizeof(int16_t), "Wrong size");
static_assert(alignof(PackedSignedBE16) == 1, "Wrong alignment");

/**
 * A packed big-endian 32 bit integer.
 */
class PackedBE32 {
	uint8_t a, b, c, d;

public:
	PackedBE32() = default;

	constexpr PackedBE32(uint32_t src) noexcept
		:a(uint8_t(src >> 24)),
		 b(uint8_t(src >> 16)),
		 c(uint8_t(src >> 8)),
		 d(uint8_t(src)) {}

	/**
	 * Construct an instance from an integer which is already
	 * big-endian.
	 */
	static constexpr auto FromBE(uint32_t src) noexcept {
		union {
			uint32_t in;
			PackedBE32 out;
		} u{src};
		return u.out;
	}

	constexpr operator uint32_t() const {
		return (uint32_t(a) << 24) | (uint32_t(b) << 16) | (uint32_t(c) << 8) | uint32_t(d);
	}

	/**
	 * Reads the raw, big-endian value.
	 */
	constexpr uint32_t raw() const noexcept {
		uint32_t x = *this;
		if (IsLittleEndian())
			x = ByteSwap32(x);
		return x;
	}

	constexpr auto operator|(PackedBE32 other) noexcept {
		PackedBE32 result{};
		result.a = a|other.a;
		result.b = b|other.b;
		result.c = c|other.c;
		result.d = d|other.d;
		return result;
	}

	constexpr auto &operator|=(PackedBE32 x) noexcept {
		return *this = *this | x;
	}
};

static_assert(sizeof(PackedBE32) == sizeof(uint32_t), "Wrong size");
static_assert(alignof(PackedBE32) == 1, "Wrong alignment");
