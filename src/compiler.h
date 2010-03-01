/*
 * uoproxy
 *
 * (c) 2005-2010 Max Kellermann <max@duempel.org>
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

#ifndef __UOPROXY_COMPILER_H
#define __UOPROXY_COMPILER_H

#if defined(__GNUC__) && __GNUC__ >= 4

/* GCC 4.x */

#define __attr_malloc __attribute__((malloc))
#define __attr_pure __attribute__((pure))
#define __attr_const __attribute__((const))
#define __attr_unused __attribute__((unused))
#define __attr_packed __attribute__((packed))
#define __attr_noreturn __attribute__((noreturn))
#define __attr_printf(string_index, first_to_check) __attribute__((format(printf, string_index, first_to_check)))

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#else

/* generic C compiler */

#define __attr_malloc
#define __attr_pure
#define __attr_const
#define __attr_unused
#define __attr_packed
#define __attr_noreturn
#define __attr_printf(string_index, first_to_check)

#define likely(x) (x)
#define unlikely(x) (x)

#endif

#endif
