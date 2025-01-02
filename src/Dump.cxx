// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Log.hxx"
#include "util/CharUtil.hxx"

#include <assert.h>
#include <stdio.h>

static constexpr char
ToPrintableChar(std::byte b) noexcept
{
	char ch = static_cast<char>(b);

	if (IsWhitespaceOrNull(ch))
		ch = ' ';
	else if (!IsASCII(ch))
		ch = '.';

	return ch;
}

static void
hexdump_line(char *dest, size_t address,
	     std::span<const std::byte> src)
{
	size_t i;

	assert(!src.empty());
	assert(src.size() <= 0x10);

	snprintf(dest, 10, "  %05x", (unsigned)address);
	dest += 7;
	*dest++ = ' ';

	for (i = 0; i < 0x10; ++i) {
		*dest++ = ' ';
		if (i == 8)
			*dest++ = ' ';

		if (i < src.size())
			snprintf(dest, 3, "%02x", static_cast<uint_least8_t>(src[i]));
		else {
			dest[0] = ' ';
			dest[1] = ' ';
		}

		dest += 2;
	}

	*dest++ = ' ';
	*dest++ = ' ';

	for (i = 0; i < src.size(); ++i) {
		if (i == 8)
			*dest++ = ' ';

		*dest++ = ToPrintableChar(src[i]);
	}

	*dest = '\0';
}

void
log_hexdump(unsigned level, std::span<const std::byte> src) noexcept
{
	if (level > verbose)
		return;

	for (size_t row = 0; !src.empty(); row += 0x10) {
		auto l = src;
		if (l.size() > 0x10)
			l = l.first(0x10);
		src = src.subspan(l.size());

		char line[80];
		hexdump_line(line, row, l);
		LogFmt(level, "{}\n", line);
	}
}
