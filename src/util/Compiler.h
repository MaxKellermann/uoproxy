// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#ifdef __GNUC__
#  define gcc_printf(string_index, first_to_check) __attribute__((format(printf, string_index, first_to_check)))
#else
#  define gcc_printf(string_index, first_to_check)
#endif
