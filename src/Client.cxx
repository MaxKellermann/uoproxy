// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Client.hxx"
#include "PacketHandler.hxx"
#include "ParsePackets.hxx"
#include "Log.hxx"
#include "uo/Command.hxx"
#include "uo/Packets.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "net/SocketProtocolError.hxx"
#include "net/UniqueSocketDescriptor.hxx"

#include <utility>

namespace UO {

Client::Client(EventLoop &event_loop, UniqueSocketDescriptor &&s,
	       uint32_t seed, const struct uo_packet_seed *seed6,
	       PacketHandler &_handler) noexcept
	:sock(event_loop, std::move(s), *this),
	 handler(_handler),
	 abort_event(event_loop, BIND_THIS_METHOD(DeferredAbort))
{
	/* seed must be the first 4 bytes, and it must be flushed */
	if (seed6 != nullptr) {
		struct uo_packet_seed p = *seed6;
		p.seed = seed;
		SendT(p);
	} else {
		PackedBE32 seed_be(seed);
		SendT(seed_be);
	}
}

Client::~Client() noexcept = default;

inline void
Client::DeferredAbort() noexcept
{
	handler.OnDisconnect();
}

} // namespace UO

void
UO::Client::Abort() noexcept
{
	/* this is a trick to delay the destruction of this object until
	   everything is done */

	abort_event.Schedule();
}

inline std::size_t
UO::Client::Decompress(std::span<const std::byte> src)
{
	decompressed_buffer.AllocateIfNull();
	auto w = decompressed_buffer.Write();
	if (w.empty())
		throw SocketBufferFullError{"Decompression buffer full"};

	decompressed_buffer.Append(decompression.Decompress(w, src));
	return src.size();
}

inline BufferedResult
UO::Client::ParsePackets(DefaultFifoBuffer &buffer)
{
	return UO::ParsePackets(buffer, protocol_version, "server", handler);
}

BufferedResult
UO::Client::OnSocketData(DefaultFifoBuffer &input_buffer)
{
	if (compression_enabled) {
		const auto compressed_src = input_buffer.Read();
		assert(!compressed_src.empty());

		if (const std::size_t nbytes = Decompress(compressed_src); nbytes == 0)
			return BufferedResult::OK;
		else
			input_buffer.Consume(nbytes);
		input_buffer.FreeIfEmpty();

		return ParsePackets(decompressed_buffer);
	} else {
		return ParsePackets(input_buffer);
	}
}

bool
UO::Client::OnSocketDisconnect() noexcept
{
	Log(2, "server closed the connection\n");
	return handler.OnDisconnect();
}

void
UO::Client::OnSocketError(std::exception_ptr error) noexcept
{
	log_error("error during communication with server", std::move(error));
	handler.OnDisconnect();
}

void
UO::Client::Send(std::span<const std::byte> src)
try {
	assert(!src.empty());

	if (IsAborted())
		return;

	LogFmt(9, "sending packet to server, length={}\n", src.size());
	log_hexdump(10, src);

	if (static_cast<UO::Command>(src.front()) == UO::Command::GameLogin)
		compression_enabled = true;

	if (!sock.Send(src))
		throw SocketBufferFullError{"Output buffer ful"};
} catch (...) {
	LogFmt(1, "error in UO::Client::Send(): {}\n", std::current_exception());
	Abort();
}
