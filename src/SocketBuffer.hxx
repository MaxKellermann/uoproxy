// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "Flush.hxx"
#include "event/SocketEvent.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/DynamicFifoBuffer.hxx"

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
	virtual size_t OnSocketData(std::span<const std::byte> src) = 0;

	/**
	 * The socket has been closed due to an error or because the peer
	 * closed his side.  sock_buff_dispose() does not trigger this
	 * callback, and the callee has to invoke this function.
	 */
	virtual void OnSocketDisconnect(int error) noexcept = 0;
};

class SocketBuffer final : PendingFlush {
	const UniqueSocketDescriptor socket;

	SocketEvent event;

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
	bool Send(const void *data, size_t length) noexcept;

	/**
	 * @return the 32-bit internet address of the socket buffer's fd, in
	 * network byte order
	 */
	uint32_t GetName() const noexcept;

	/**
	 * @return the 16-bit port ofthe socket buffer's fd, in network byte
	 * order
	 */
	uint16_t GetPort() const noexcept;

	using PendingFlush::ScheduleFlush;

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

	/* virtual methods from PendingFlush */
	void DoFlush() noexcept override;
};
