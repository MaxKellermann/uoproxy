// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

class UniqueSocketDescriptor;
class SocketAddress;

/**
 * Throws on error.
 */
[[nodiscard]]
UniqueSocketDescriptor
setup_server_socket(SocketAddress bind_address);
