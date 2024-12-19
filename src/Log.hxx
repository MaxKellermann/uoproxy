// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <fmt/core.h>

#include <exception>

#include <string.h>
#include <errno.h>
#include <stddef.h>

#ifdef DISABLE_LOGGING
#define Log(level, msg)
#define log_error(msg, error)
#define log_errno(msg)
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

static inline void
log_error(const char *msg, int error)
{
	if (error <= 0)
		LogFmt(1, "{}: {}\n", msg, error);
	else
		LogFmt(1, "{}: {}\n", msg, strerror(error));
}

static inline void
log_errno(const char *msg)
{
	LogFmt(1, "{}: {}\n", msg, strerror(errno));
}

void
log_hexdump(unsigned level, const void *data, size_t length) noexcept;

#endif
