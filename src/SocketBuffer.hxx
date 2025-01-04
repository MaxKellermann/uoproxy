// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "event/net/BufferedSocket.hxx"
#include "DefaultFifoBuffer.hxx"

#include <cstddef>
#include <exception>

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
	 *
	 * @return true if the #SocketBuffer has been destroyed
	 */
	virtual bool OnSocketDisconnect() noexcept = 0;

	/**
	 * The socket has been closed due to an error.
	 */
	virtual void OnSocketError(std::exception_ptr error) noexcept = 0;
};

class SocketBuffer final
	: BufferedSocketHandler
{
	BufferedSocket socket;

	DefaultFifoBuffer output;

	SocketBufferHandler &handler;

public:
	SocketBuffer(EventLoop &event_loop, UniqueSocketDescriptor &&s,
		     SocketBufferHandler &_handler);
	~SocketBuffer() noexcept;

	std::span<std::byte> Write() noexcept {
		output.AllocateIfNull();
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

private:
	// virtual methods from BufferedSocketHandler
	BufferedResult OnBufferedData() noexcept override;
	bool OnBufferedClosed() noexcept override;
	bool OnBufferedWrite() override;
	void OnBufferedError(std::exception_ptr e) noexcept override;
};
