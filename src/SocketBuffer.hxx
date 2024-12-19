// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

class UniqueSocketDescriptor;
class EventLoop;

class SocketBufferHandler {
public:
	/**
	 * Data is available.
	 *
	 * @return the number of bytes consumed, or 0 if the sock_buff
	 * has been closed within the function
	 */
	virtual size_t OnSocketData(const void *data, size_t length) = 0;

	/**
	 * The socket has been closed due to an error or because the peer
	 * closed his side.  sock_buff_dispose() does not trigger this
	 * callback, and the callee has to invoke this function.
	 */
	virtual void OnSocketDisconnect(int error) noexcept = 0;
};

struct SocketBuffer;

SocketBuffer *
sock_buff_create(EventLoop &event_loop, UniqueSocketDescriptor &&s, size_t input_max,
		 size_t output_max,
		 SocketBufferHandler &handler);

void sock_buff_dispose(SocketBuffer *sb);

std::span<std::byte>
sock_buff_write(SocketBuffer *sb) noexcept;

void
sock_buff_append(SocketBuffer *sb, size_t length);

/**
 * @return true on success, false if there is no more room in the
 * output buffer
 */
bool
sock_buff_send(SocketBuffer *sb, const void *data, size_t length);

/**
 * @return the 32-bit internet address of the socket buffer's fd, in
 * network byte order
 */
uint32_t sock_buff_sockname(const SocketBuffer *sb);

/**
 * @return the 16-bit port ofthe socket buffer's fd, in network byte
 * order
 */
uint16_t sock_buff_port(const SocketBuffer *sb);
