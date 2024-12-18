// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#ifdef __linux

int
socket_set_nodelay(int fd, int value);

#else

static inline int
socket_set_nodelay(int, int) noexcept
{
    return 0;
}

#endif
