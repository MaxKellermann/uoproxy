// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Log.hxx"
#include "util/Exception.hxx"

unsigned verbose = 1;

void
log_error(const char *msg, std::exception_ptr error) noexcept
{
    LogFmt(1, "{}: {}\n", msg, GetFullMessage(std::move(error)).c_str());
}
