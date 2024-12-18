// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

class SocketAddress;

int
socket_connect(int domain, int type, int protocol,
	       SocketAddress address);
