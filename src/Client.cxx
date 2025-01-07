// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Client.hxx"
#include "PacketHandler.hxx"
#include "Log.hxx"
#include "uo/Command.hxx"
#include "uo/Length.hxx"
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

ssize_t
UO::Client::ParsePackets(std::span<const std::byte> src)
{
	size_t consumed = 0, packet_length;

	while (!src.empty()) {
		packet_length = GetPacketLength(src, protocol_version);
		if (packet_length == PACKET_LENGTH_INVALID) {
			Log(1, "malformed packet from server\n");
			log_hexdump(5, src);
			throw SocketProtocolError{fmt::format("Malformed {:#02x} packet", src.front())};
		}

		LogFmt(9, "from server: {:#02x} length={}\n",
		       src.front(), packet_length);

		if (packet_length == 0 || packet_length > src.size())
			break;

		const auto packet = src.first(packet_length);

		log_hexdump(10, packet);

		switch (handler.OnPacket(packet)) {
		case PacketHandler::OnPacketResult::OK:
			break;

		case PacketHandler::OnPacketResult::BLOCKING:
			return static_cast<ssize_t>(consumed);

		case PacketHandler::OnPacketResult::CLOSED:
			return -1;
		}

		consumed += packet_length;
		src = src.subspan(packet_length);
	}

	return (ssize_t)consumed;
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

		auto r = decompressed_buffer.Read();
		if (r.empty())
			return BufferedResult::OK;

		const auto nbytes = ParsePackets(r);
		if (nbytes < 0)
			return BufferedResult::DESTROYED;

		decompressed_buffer.Consume((size_t)nbytes);
		decompressed_buffer.FreeIfEmpty();

		return BufferedResult::OK;
	} else {
		const auto src = input_buffer.Read();
		assert(!src.empty());

		ssize_t nbytes = ParsePackets(src);
		if (nbytes < 0)
			return BufferedResult::DESTROYED;

		input_buffer.Consume(static_cast<std::size_t>(nbytes));
		input_buffer.FreeIfEmpty();

		return BufferedResult::OK;
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
