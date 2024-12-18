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
	virtual bool OnServerPacket(const void *data, size_t length) = 0;

	/**
	 * The connection has been closed due to an error or because the
	 * peer closed his side.  uo_server_dispose() does not trigger
	 * this callback, and the method has to invoke this function.
	 */
	virtual void OnServerDisconnect() noexcept = 0;
};

class Server final : SocketBufferHandler  {
	SocketBuffer *const sock;
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

	/** @return ip address, in network byte order, of our uo server socket
	    (= connection to client) */
	uint32_t GetSockName() const noexcept {
		return sock_buff_sockname(sock);
	}

	/** @return port, in network byte order, of our uo server socket
	    (= connection to client) */
	uint16_t GetSockPort() const noexcept {
		return sock_buff_port(sock);
	}

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

	void Send(const void *src, size_t length);

	void SpeakAscii(uint32_t serial,
			int16_t graphic,
			uint8_t type,
			uint16_t hue, uint16_t font,
			const char *name,
			const char *text);

	void SpeakConsole(const char *text);

private:
	void DeferredAbort() noexcept;

	ssize_t ParsePackets(const uint8_t *data, size_t length);

	/* virtual methods from SocketBufferHandler */
	size_t OnSocketData(const void *data, size_t length) override;
	void OnSocketDisconnect(int error) noexcept override;
};

} // namespace UO
