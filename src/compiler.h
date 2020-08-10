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

#ifndef __UOPROXY_COMPILER_H
#define __UOPROXY_COMPILER_H

#if defined(__GNUC__) && __GNUC__ >= 4

/* GCC 4.x */

#define gcc_unused __attribute__((unused))
#define gcc_printf(string_index, first_to_check) __attribute__((format(printf, string_index, first_to_check)))

#else

/* generic C compiler */

#define gcc_unused
#define gcc_printf(string_index, first_to_check)

#endif

#endif
