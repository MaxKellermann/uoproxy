// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

class SocketAddress;

/**
 * Throws on error.
 */
int
socket_connect(int domain, int type, int protocol,
	       SocketAddress address);
