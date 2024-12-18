// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Log.hxx"
#include "util/Exception.hxx"

#include <stdio.h>
#include <stdarg.h>

unsigned verbose = 1;

void
do_log(const char *fmt, ...) noexcept
{
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    fflush(stdout);
}

void
log_error(const char *msg, std::exception_ptr error) noexcept
{
    LogFormat(1, "%s: %s\n", msg, GetFullMessage(std::move(error)).c_str());
}
