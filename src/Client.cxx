// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Client.hxx"
#include "PacketLengths.hxx"
#include "PacketStructs.hxx"
#include "Log.hxx"
#include "uo/Command.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "net/SocketProtocolError.hxx"
#include "net/UniqueSocketDescriptor.hxx"

#include <utility>

namespace UO {

Client::Client(EventLoop &event_loop, UniqueSocketDescriptor &&s,
	       uint32_t seed, const struct uo_packet_seed *seed6,
	       ClientHandler &_handler) noexcept
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
	handler.OnClientDisconnect();
}

} // namespace UO

void
UO::Client::Abort() noexcept
{
	/* this is a trick to delay the destruction of this object until
	   everything is done */

	abort_event.Schedule();
}

inline ssize_t
UO::Client::Decompress(std::span<const std::byte> src)
{
	decompressed_buffer.AllocateIfNull();
	auto w = decompressed_buffer.Write();
	if (w.empty()) {
		Log(1, "decompression buffer full\n");
		Abort();
		return -1;
	}

	decompressed_buffer.Append(decompression.Decompress(w, src));
	return src.size();
}

ssize_t
UO::Client::ParsePackets(std::span<const std::byte> src)
{
	size_t consumed = 0, packet_length;

	while (!src.empty()) {
		packet_length = get_packet_length(protocol_version, src.data(), src.size());
		if (packet_length == PACKET_LENGTH_INVALID) {
			Log(1, "malformed packet from server\n");
			log_hexdump(5, src);
			Abort();
			return 0;
		}

		LogFmt(9, "from server: {:#02x} length={}\n",
		       src.front(), packet_length);

		if (packet_length == 0 || packet_length > src.size())
			break;

		const auto packet = src.first(packet_length);

		log_hexdump(10, packet);

		if (!handler.OnClientPacket(packet))
			return -1;

		consumed += packet_length;
		src = src.subspan(packet_length);
	}

	return (ssize_t)consumed;
}

size_t
UO::Client::OnSocketData(std::span<const std::byte> src)
{
	if (compression_enabled) {
		ssize_t nbytes;
		size_t consumed;

		nbytes = Decompress(src);
		if (nbytes <= 0)
			return 0;
		consumed = (size_t)nbytes;

		auto r = decompressed_buffer.Read();
		if (r.empty())
			return consumed;

		nbytes = ParsePackets(std::as_bytes(r));
		if (nbytes < 0)
			return 0;

		decompressed_buffer.Consume((size_t)nbytes);
		decompressed_buffer.FreeIfEmpty();

		return consumed;
	} else {
		ssize_t nbytes = ParsePackets(src);
		if (nbytes < 0)
			return 0;

		return nbytes;
	}
}

bool
UO::Client::OnSocketDisconnect() noexcept
{
	Log(2, "server closed the connection\n");
	return handler.OnClientDisconnect();
}

void
UO::Client::OnSocketError(std::exception_ptr error) noexcept
{
	log_error("error during communication with server", std::move(error));
	handler.OnClientDisconnect();
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
