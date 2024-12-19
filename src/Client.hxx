// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "SocketBuffer.hxx"
#include "Compression.hxx"
#include "PVersion.hxx"
#include "event/DeferEvent.hxx"
#include "util/DynamicFifoBuffer.hxx"

#include <cassert>
#include <cstdint>
#include <cstddef>

class UniqueSocketDescriptor;
class EventLoop;
struct uo_packet_seed;

namespace UO {

class ClientHandler {
public:
	/**
	 * A packet has been received.
	 *
	 * @return false if this object has been closed within the
	 * function
	 */
	virtual bool OnClientPacket(const void *data, size_t length) = 0;

	/**
	 * The connection has been closed due to an error or because the
	 * peer closed his side.  uo_client_dispose() does not trigger
	 * this callback, and the method has to invoke this function.
	 */
	virtual void OnClientDisconnect() noexcept = 0;
};

class Client final : SocketBufferHandler {
	SocketBuffer *const sock;
	bool compression_enabled = false;
	UO::Decompression decompression;
	DynamicFifoBuffer<uint8_t> decompressed_buffer{65536};

	enum protocol_version protocol_version = PROTOCOL_UNKNOWN;

	ClientHandler &handler;

	DeferEvent abort_event;

public:
	explicit Client(EventLoop &event_loop, UniqueSocketDescriptor &&s,
			uint32_t seed, const struct uo_packet_seed *seed6,
			ClientHandler &_handler) noexcept;

	~Client() noexcept;

	void Abort() noexcept;

	void SetProtocol(enum protocol_version _protocol_version) noexcept {
		assert(protocol_version == PROTOCOL_UNKNOWN);

		protocol_version = _protocol_version;
	}

	void Send(const void *src, size_t length);

private:
	void DeferredAbort() noexcept;

	ssize_t Decompress(std::span<const uint8_t> src);
	ssize_t ParsePackets(const uint8_t *data, size_t length);

	/* virtual methods from SocketBufferHandler */
	size_t OnSocketData(const void *data, size_t length) override;
	void OnSocketDisconnect(int error) noexcept override;
};

} // namespace UO
