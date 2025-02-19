// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Server.hxx"
#include "PacketHandler.hxx"
#include "ParsePackets.hxx"
#include "Log.hxx"
#include "uo/Length.hxx"
#include "uo/Packets.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "net/IPv4Address.hxx"
#include "net/SocketProtocolError.hxx"

#include <utility>

namespace UO {

Server::Server(EventLoop &event_loop,
               UniqueSocketDescriptor &&s, PacketHandler &_handler) noexcept
	:sock(event_loop, std::move(s), *this),
	handler(_handler),
	abort_event(event_loop, BIND_THIS_METHOD(DeferredAbort))
{
}

Server::~Server() noexcept = default;

inline void
Server::DeferredAbort() noexcept
{
	handler.OnDisconnect();
}

} // namespace UO

void
UO::Server::Abort() noexcept
{
	/* this is a trick to delay the destruction of this object until
	   everything is done */

	abort_event.Schedule();
}

IPv4Address
UO::Server::GetLocalIPv4Address() const noexcept
{
	return sock.GetLocalIPv4Address();
}

inline BufferedResult
UO::Server::ParsePackets(DefaultFifoBuffer &buffer)
{
	return UO::ParsePackets(buffer, protocol_version, "client", handler);
}

BufferedResult
UO::Server::OnSocketData(DefaultFifoBuffer &_input_buffer)
{
	auto &input_buffer = encryption.FromClient(_input_buffer);
	std::span<const std::byte> src = input_buffer.Read();
	assert(!src.empty());

	size_t consumed = 0;

	if (seed == 0 && src.front() == std::byte{0xef}) {
		/* client 6.0.5.0 sends a "0xef" seed packet instead of the
		   raw 32 bit seed */
		const auto *p = reinterpret_cast<const struct uo_packet_seed *>(src.data());

		if (src.size() < sizeof(*p))
			return BufferedResult::MORE;

		seed = p->seed;
		if (seed == 0)
			throw SocketProtocolError{"Zero seed from client"};
	}

	if (seed == 0) {
		/* the first packet from a client is the seed, 4 bytes without
		   header */
		if (src.size() < 4)
			return BufferedResult::MORE;

		seed = *(const uint32_t*)src.data();
		if (seed == 0)
			throw SocketProtocolError{"Zero seed from client"};

		consumed += sizeof(uint32_t);
		src = src.subspan(sizeof(uint32_t));
	}

	input_buffer.Consume(consumed);

	return ParsePackets(input_buffer);
}

bool
UO::Server::OnSocketDisconnect() noexcept
{
	Log(2, "client closed the connection\n");
	return handler.OnDisconnect();
}

void
UO::Server::OnSocketError(std::exception_ptr error) noexcept
{
	log_error("error during communication with client", std::move(error));
	handler.OnDisconnect();
}

void
UO::Server::Send(std::span<const std::byte> src)
try {
	assert(!src.empty());
	assert(GetPacketLength(src, protocol_version) == src.size());

	if (IsAborted())
		return;

	LogFmt(9, "sending packet to client, length={}\n", src.size());
	log_hexdump(10, src);

	if (compression_enabled) {
		auto w = sock.Write();
		if (w.empty())
			throw SocketBufferFullError{"Output buffer ful"};

		sock.Append(UO::Compress(w, src));
	} else {
		if (!sock.Send(src))
			throw SocketBufferFullError{"Output buffer ful"};
	}
} catch (...) {
	LogFmt(1, "error in UO::Server::Send(): {}\n", std::current_exception());
	Abort();
}
