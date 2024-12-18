// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Log.hxx"

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
