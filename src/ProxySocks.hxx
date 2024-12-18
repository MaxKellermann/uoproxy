// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

class SocketDescriptor;
class SocketAddress;

/**
 * Throws on error.
 */
void
socks_connect(SocketDescriptor s, SocketAddress address);
