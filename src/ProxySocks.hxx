// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

class EventLoop;
class SocketDescriptor;
class SocketAddress;
namespace Co { template<typename T> class Task; }

/**
 * Throws on error.
 */
[[nodiscard]]
Co::Task<void>
ProxySocksConnect(EventLoop &event_loop, SocketDescriptor s, SocketAddress address);
