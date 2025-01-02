// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <fmt/core.h>

#include <exception>

#include <stddef.h>

#ifdef DISABLE_LOGGING
#define Log(level, msg)
#define log_error(msg, error)
#define log_hexdump(level, data, length)
#else

extern unsigned verbose;

inline void
Log(unsigned level, const char *msg) noexcept
{
	if (verbose >= level)
		fputs(msg, stderr);
}

#define LogFmt(level, ...) do { if (verbose >= (level)) fmt::print(stderr, __VA_ARGS__); } while (0)

void
log_error(const char *msg, std::exception_ptr error) noexcept;

void
log_hexdump(unsigned level, const void *data, size_t length) noexcept;

#endif
