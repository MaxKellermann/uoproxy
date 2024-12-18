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
