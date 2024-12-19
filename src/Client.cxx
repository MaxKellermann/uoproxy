// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Client.hxx"
#include "PacketLengths.hxx"
#include "PacketStructs.hxx"
#include "Log.hxx"
#include "uo/Command.hxx"
#include "net/UniqueSocketDescriptor.hxx"

#include <utility>

namespace UO {

Client::Client(EventLoop &event_loop, UniqueSocketDescriptor &&s,
	       uint32_t seed, const struct uo_packet_seed *seed6,
	       ClientHandler &_handler) noexcept
	:sock(event_loop, std::move(s), 8192, 65536, *this),
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
UO::Client::Decompress(std::span<const uint8_t> src)
{
	auto w = decompressed_buffer.Write();
	if (w.empty()) {
		Log(1, "decompression buffer full\n");
		Abort();
		return -1;
	}

	ssize_t nbytes = decompression.Decompress(w.data(), w.size(), src);
	if (nbytes < 0) {
		Log(1, "decompression failed\n");
		Abort();
		return -1;
	}

	decompressed_buffer.Append((size_t)nbytes);

	return src.size();
}

ssize_t
UO::Client::ParsePackets(const uint8_t *data, size_t length)
{
	size_t consumed = 0, packet_length;

	while (length > 0) {
		packet_length = get_packet_length(protocol_version, data, length);
		if (packet_length == PACKET_LENGTH_INVALID) {
			Log(1, "malformed packet from server\n");
			log_hexdump(5, data, length);
			Abort();
			return 0;
		}

		LogFmt(9, "from server: {:#02x} length={}\n",
		       data[0], packet_length);

		if (packet_length == 0 || packet_length > length)
			break;

		log_hexdump(10, data, packet_length);

		if (!handler.OnClientPacket(std::as_bytes(std::span{data, packet_length})))
			return -1;

		consumed += packet_length;
		data += packet_length;
		length -= packet_length;
	}

	return (ssize_t)consumed;
}

size_t
UO::Client::OnSocketData(std::span<const std::byte> src)
{
	const uint8_t *data = (const uint8_t *)src.data();

	if (compression_enabled) {
		ssize_t nbytes;
		size_t consumed;

		nbytes = Decompress({data, src.size()});
		if (nbytes <= 0)
			return 0;
		consumed = (size_t)nbytes;

		auto r = decompressed_buffer.Read();
		if (r.empty())
			return consumed;

		nbytes = ParsePackets(r.data(), r.size());
		if (nbytes < 0)
			return 0;

		decompressed_buffer.Consume((size_t)nbytes);

		return consumed;
	} else {
		ssize_t nbytes = ParsePackets(reinterpret_cast<const uint8_t *>(src.data()), src.size());
		if (nbytes < 0)
			return 0;

		return nbytes;
	}
}

void
UO::Client::OnSocketDisconnect(int error) noexcept
{
	if (error == 0)
		Log(2, "server closed the connection\n");
	else
		log_error("error during communication with server", error);

	handler.OnClientDisconnect();
}

void
UO::Client::Send(std::span<const std::byte> src)
{
	assert(!src.empty());

	if (abort_event.IsPending())
		return;

	LogFmt(9, "sending packet to server, length={}\n", src.size());
	log_hexdump(10, src.data(), src.size());

	if (static_cast<UO::Command>(src.front()) == UO::Command::GameLogin)
		compression_enabled = true;

	if (!sock.Send(src.data(), src.size())) {
		Log(1, "output buffer full in uo_client_send()\n");
		Abort();
	}
}
