// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "SocketBuffer.hxx"
#include "Compression.hxx"
#include "Encryption.hxx"
#include "PVersion.hxx"
#include "event/DeferEvent.hxx"

#include <cassert>
#include <cstdint>
#include <cstddef>
#include <span>
#include <string_view>
#include <type_traits>

class UniqueSocketDescriptor;
class EventLoop;

namespace UO {

class ServerHandler {
public:
	/**
	 * A packet has been received.
	 *
	 * @return false if this object has been closed within the
	 * function
	 */
	virtual bool OnServerPacket(std::span<const std::byte> src) = 0;

	/**
	 * The connection has been closed due to an error or because the
	 * peer closed his side.  uo_server_dispose() does not trigger
	 * this callback, and the method has to invoke this function.
	 */
	virtual void OnServerDisconnect() noexcept = 0;
};

class Server final : SocketBufferHandler  {
	SocketBuffer sock;
	uint32_t seed = 0;
	bool compression_enabled = false;

	UO::Encryption encryption;

	enum protocol_version protocol_version = PROTOCOL_UNKNOWN;

	ServerHandler &handler;

	DeferEvent abort_event;

public:
	Server(EventLoop &event_loop,
               UniqueSocketDescriptor &&s, ServerHandler &_handler) noexcept;

	~Server() noexcept;

	void Abort() noexcept;

	bool IsAborted() const noexcept {
		return abort_event.IsPending();
	}

	[[gnu::pure]]
	IPv4Address GetLocalIPv4Address() const noexcept;

	uint32_t GetSeed() const noexcept {
		return seed;
	}

	void SetProtocol(enum protocol_version _protocol_version) noexcept {
		assert(protocol_version == PROTOCOL_UNKNOWN);

		protocol_version = _protocol_version;
	}

	void SetCompression(bool _compression_enabled) noexcept {
		compression_enabled = _compression_enabled;
	}

	void Send(std::span<const std::byte> src);

	template<typename T>
	requires std::has_unique_object_representations_v<T>
	void SendT(const T &src) {
		Send(std::as_bytes(std::span{&src, 1}));
	}

	void SpeakAscii(uint32_t serial,
			int16_t graphic,
			uint8_t type,
			uint16_t hue, uint16_t font,
			std::string_view name,
			std::string_view text);

	void SpeakConsole(std::string_view text);

private:
	void DeferredAbort() noexcept;

	ssize_t ParsePackets(std::span<const std::byte> src);

	/* virtual methods from SocketBufferHandler */
	size_t OnSocketData(std::span<const std::byte> src) override;
	void OnSocketDisconnect() noexcept override;
	void OnSocketError(std::exception_ptr error) noexcept override;
};

} // namespace UO
