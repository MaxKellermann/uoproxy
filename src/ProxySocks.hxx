// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

class SocketAddress;

bool
socks_connect(int fd, SocketAddress address);
