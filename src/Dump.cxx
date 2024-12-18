// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Log.hxx"

#include <assert.h>
#include <stdio.h>

static void
hexdump_line(char *dest, size_t address,
             const unsigned char *data, size_t length)
{
    size_t i;

    assert(length > 0);
    assert(length <= 0x10);

    snprintf(dest, 10, "  %05x", (unsigned)address);
    dest += 7;
    *dest++ = ' ';

    for (i = 0; i < 0x10; ++i) {
        *dest++ = ' ';
        if (i == 8)
            *dest++ = ' ';

        if (i < length)
            snprintf(dest, 3, "%02x", data[i]);
        else {
            dest[0] = ' ';
            dest[1] = ' ';
        }

        dest += 2;
    }

    *dest++ = ' ';
    *dest++ = ' ';

    for (i = 0; i < length; ++i) {
        if (i == 8)
            *dest++ = ' ';

        if (data[i] <= ' ')
            *dest++ = ' ';
        else if (data[i] < 0x80)
            *dest++ = (char)data[i];
        else
            *dest++ = '.';
    }

    *dest = '\0';
}

static size_t
min_size_t(size_t a, size_t b)
{
    return a < b ? a : b;
}

void
log_hexdump(unsigned level, const void *data, size_t length) noexcept
{
    const unsigned char *p = (const unsigned char *)data;
    size_t row;
    char line[80];

    if (level > verbose)
        return;

    for (row = 0; row < length; row += 0x10) {
        hexdump_line(line, row, p + row,
                     min_size_t(0x10, length - row));
        LogFmt(level, "{}\n", line);
    }
}
