/*
 * uoproxy
 *
 * (c) 2005-2007 Max Kellermann <max@duempel.org>
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

#include <stdio.h>
#include <string.h>
#include <errno.h>

extern int verbose;

#ifdef DISABLE_LOGGING
#define log(level, ...)
#define log_oom()
#define log_error(msg, error)
#define log_errno(msg)
#else

#define log(level, ...) do { if (verbose >= (level)) { printf(__VA_ARGS__); fflush(stdout); } } while (0)

static inline void
log_oom(void)
{
    log(1, "Out of memory\n");
}

static inline void
log_error(const char *msg, int error)
{
    if (error <= 0)
        log(1, "%s: %d\n", msg, error);
    else
        log(1, "%s: %s\n", msg, strerror(error));
}

static inline void
log_errno(const char *msg)
{
    log(1, "%s: %s\n", msg, strerror(errno));
}

#endif

#endif
