// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "SocketBuffer.hxx"
#include "Compression.hxx"
#include "uo/Version.hxx"
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

class PacketHandler;

class Client final : SocketBufferHandler {
	SocketBuffer sock;
	UO::Decompression decompression;
	DefaultFifoBuffer decompressed_buffer;

	PacketHandler &handler;

	DeferEvent abort_event;

	ProtocolVersion protocol_version = ProtocolVersion::UNKNOWN;

	bool compression_enabled = false;

public:
	explicit Client(EventLoop &event_loop, UniqueSocketDescriptor &&s,
			uint32_t seed, const struct uo_packet_seed *seed6,
			PacketHandler &_handler) noexcept;

	~Client() noexcept;

	void Abort() noexcept;

	bool IsAborted() const noexcept {
		return abort_event.IsPending();
	}

	void SetProtocol(ProtocolVersion _protocol_version) noexcept {
		assert(protocol_version == ProtocolVersion::UNKNOWN);

		protocol_version = _protocol_version;
	}

	void ScheduleRead() noexcept {
		sock.ScheduleRead();
	}

	void Send(std::span<const std::byte> src);

	template<typename T>
	requires std::has_unique_object_representations_v<T>
	void SendT(const T &src) {
		Send(std::as_bytes(std::span{&src, 1}));
	}

private:
	void DeferredAbort() noexcept;

	std::size_t Decompress(std::span<const std::byte> src);
	BufferedResult ParsePackets(DefaultFifoBuffer &buffer);

	/* virtual methods from SocketBufferHandler */
	BufferedResult OnSocketData(DefaultFifoBuffer &input_buffer) override;
	bool OnSocketDisconnect() noexcept override;
	void OnSocketError(std::exception_ptr error) noexcept override;
};

} // namespace UO
