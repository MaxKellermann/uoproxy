// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "event/DeferEvent.hxx"
#include "event/SocketEvent.hxx"
#include "util/DynamicFifoBuffer.hxx"

#include <cstddef>
#include <exception>
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
	virtual size_t OnSocketData(std::span<const std::byte> src) = 0;

	/**
	 * The socket has been closed because the peer closed his
	 * side.
	 */
	virtual void OnSocketDisconnect() noexcept = 0;

	/**
	 * The socket has been closed due to an error.
	 */
	virtual void OnSocketError(std::exception_ptr error) noexcept = 0;
};

class SocketBuffer final {
	SocketEvent event;

	DeferEvent defer_send;

	DynamicFifoBuffer<std::byte> input, output;

	SocketBufferHandler &handler;

public:
	SocketBuffer(EventLoop &event_loop, UniqueSocketDescriptor &&s, size_t input_max,
		     size_t output_max,
		     SocketBufferHandler &_handler);
	~SocketBuffer() noexcept;

	std::span<std::byte> Write() noexcept {
		return output.Write();
	}

	void Append(std::size_t length) noexcept;

	/**
	 * @return true on success, false if there is no more room in the
	 * output buffer
	 */
	bool Send(std::span<const std::byte> src) noexcept;

	/**
	 * Determines the local IPv4 address this socket is bound to.
	 */
	[[gnu::pure]]
	IPv4Address GetLocalIPv4Address() const noexcept;

	/**
	 * @return false on error or if nothing was consumed
	 */
	bool SubmitData();

	/**
	 * Try to flush the output buffer.  Note that this function will
	 * not trigger the free() callback.
	 *
	 * @return true on success, false on i/o error (see errno)
	 */
	bool FlushOutput();

protected:
	void OnSocketReady(unsigned events) noexcept;
	void OnDeferredSend() noexcept;
};
