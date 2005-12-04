/*
 * uoproxy
 * $Id$
 *
 * (c) 2005 Max Kellermann <max@duempel.org>
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

#include <stdio.h>

#include "dump.h"

void fhexdump(FILE *stream, const char *indent,
              const void *data, size_t length) {
    const unsigned char *p = data;
    size_t row, column;

    for (row = 0; row < length; row += 0x10) {
        fprintf(stream, "%s0x%05zx", indent, row);

        putc(' ', stream);

        for (column = 0; column < 0x10; column++) {
            if (column == 8)
                putc(' ', stream);
            if (row + column < length)
                fprintf(stream, " %02x", p[row + column]);
            else
                fputs("   ", stream);
        }

        fputs("  ", stream);

        for (column = 0; column < 0x10 && row + column < length; column++) {
            const unsigned char ch = p[row + column];

            if (column == 8)
                putc(' ', stream);

            if (ch <= ' ')
                putc(' ', stream);
            else if (ch < 0x80)
                putc(ch, stream);
            else
                putc('.', stream);
        }

        putc('\n', stream);
    }
}
