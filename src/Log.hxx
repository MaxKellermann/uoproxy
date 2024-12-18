// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "util/Compiler.h"

#include <exception>

#include <string.h>
#include <errno.h>
#include <stddef.h>

#ifdef DISABLE_LOGGING
#define LogFormat(level, ...)
#define log_oom()
#define log_error(msg, error)
#define log_errno(msg)
#define log_hexdump(level, data, length)
#else

extern unsigned verbose;

void
do_log(const char *fmt, ...) noexcept
    gcc_printf(1, 2);

#define LogFormat(level, ...) do { if (verbose >= (level)) do_log(__VA_ARGS__); } while (0)

static inline void
log_oom()
{
    LogFormat(1, "Out of memory\n");
}

void
log_error(const char *msg, std::exception_ptr error) noexcept;

static inline void
log_error(const char *msg, int error)
{
    if (error <= 0)
        LogFormat(1, "%s: %d\n", msg, error);
    else
        LogFormat(1, "%s: %s\n", msg, strerror(error));
}

static inline void
log_errno(const char *msg)
{
    LogFormat(1, "%s: %s\n", msg, strerror(errno));
}

void
log_hexdump(unsigned level, const void *data, size_t length) noexcept;

#endif
