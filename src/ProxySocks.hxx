// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

struct sockaddr;

bool
socks_connect(int fd, const struct sockaddr *address);
