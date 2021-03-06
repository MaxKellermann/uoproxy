/*
 * uoproxy
 *
 * Copyright 2005-2020 Max Kellermann <max.kellermann@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef __UOPROXY_LOG_H
#define __UOPROXY_LOG_H

#include "util/Compiler.h"

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

#endif
