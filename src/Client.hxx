// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "SocketBuffer.hxx"
#include "Compression.hxx"
#include "PVersion.hxx"
#include "event/DeferEvent.hxx"
#include "DefaultFifoBuffer.hxx"

#include <cassert>
#include <cstdint>
#include <cstddef>
#include <span>
#include <type_traits>

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
	virtual bool OnClientPacket(std::span<const std::byte> src) = 0;

	/**
	 * The connection has been closed due to an error or because the
	 * peer closed his side.  uo_client_dispose() does not trigger
	 * this callback, and the method has to invoke this function.
	 */
	virtual void OnClientDisconnect() noexcept = 0;
};

class Client final : SocketBufferHandler {
	SocketBuffer sock;
	bool compression_enabled = false;
	UO::Decompression decompression;
	DefaultFifoBuffer decompressed_buffer;

	enum protocol_version protocol_version = PROTOCOL_UNKNOWN;

	ClientHandler &handler;

	DeferEvent abort_event;

public:
	explicit Client(EventLoop &event_loop, UniqueSocketDescriptor &&s,
			uint32_t seed, const struct uo_packet_seed *seed6,
			ClientHandler &_handler) noexcept;

	~Client() noexcept;

	void Abort() noexcept;

	bool IsAborted() const noexcept {
		return abort_event.IsPending();
	}

	void SetProtocol(enum protocol_version _protocol_version) noexcept {
		assert(protocol_version == PROTOCOL_UNKNOWN);

		protocol_version = _protocol_version;
	}

	void Send(std::span<const std::byte> src);

	template<typename T>
	requires std::has_unique_object_representations_v<T>
	void SendT(const T &src) {
		Send(std::as_bytes(std::span{&src, 1}));
	}

private:
	void DeferredAbort() noexcept;

	ssize_t Decompress(std::span<const std::byte> src);
	ssize_t ParsePackets(std::span<const std::byte> src);

	/* virtual methods from SocketBufferHandler */
	size_t OnSocketData(std::span<const std::byte> src) override;
	void OnSocketDisconnect() noexcept override;
	void OnSocketError(std::exception_ptr error) noexcept override;
};

} // namespace UO
