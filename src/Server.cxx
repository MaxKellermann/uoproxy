// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Server.hxx"
#include "PacketLengths.hxx"
#include "PacketStructs.hxx"
#include "Log.hxx"

#include <utility>

namespace UO {

Server::Server(EventLoop &event_loop,
               UniqueSocketDescriptor &&s, ServerHandler &_handler) noexcept
	:sock(event_loop, std::move(s), 8192, 65536, *this),
	handler(_handler),
	abort_event(event_loop, BIND_THIS_METHOD(DeferredAbort))
{
}

Server::~Server() noexcept = default;

inline void
Server::DeferredAbort() noexcept
{
	handler.OnServerDisconnect();
}

} // namespace UO

void
UO::Server::Abort() noexcept
{
	/* this is a trick to delay the destruction of this object until
	   everything is done */

	abort_event.Schedule();
}

inline ssize_t
UO::Server::ParsePackets(const uint8_t *data, size_t length)
{
	size_t consumed = 0;

	while (length > 0) {
		size_t packet_length = get_packet_length(protocol_version,
							 data, length);
		if (packet_length == PACKET_LENGTH_INVALID) {
			Log(1, "malformed packet from client\n");
			log_hexdump(5, data, length);
			Abort();
			return 0;
		}

		LogFmt(9, "from client: {:#02x} length={}\n",
		       data[0], packet_length);

		if (packet_length == 0 || packet_length > length)
			break;

		log_hexdump(10, data, packet_length);

		if (!handler.OnServerPacket(std::as_bytes(std::span{data, packet_length})))
			return -1;

		consumed += packet_length;
		data += packet_length;
		length -= packet_length;
	}

	return (ssize_t)consumed;
}

size_t
UO::Server::OnSocketData(std::span<const std::byte> src)
{
	assert(!src.empty());

	if (const void *decrypted = encryption.FromClient(src.data(), src.size());
	    decrypted == nullptr)
		/* need more data */
		return 0;
	else
		src = {reinterpret_cast<const std::byte *>(decrypted), src.size()};

	size_t consumed = 0;

	if (seed == 0 && src.front() == std::byte{0xef}) {
		/* client 6.0.5.0 sends a "0xef" seed packet instead of the
		   raw 32 bit seed */
		const auto *p = reinterpret_cast<const struct uo_packet_seed *>(src.data());

		if (src.size() < sizeof(*p))
			return 0;

		seed = p->seed;
		if (seed == 0) {
			Log(2, "zero seed from client\n");
			Abort();
			return 0;
		}
	}

	if (seed == 0) {
		/* the first packet from a client is the seed, 4 bytes without
		   header */
		if (src.size() < 4)
			return 0;

		seed = *(const uint32_t*)src.data();
		if (seed == 0) {
			Log(2, "zero seed from client\n");
			Abort();
			return 0;
		}

		consumed += sizeof(uint32_t);
		src = src.subspan(sizeof(uint32_t));
	}

	ssize_t nbytes = ParsePackets(reinterpret_cast<const uint8_t *>(src.data()), src.size());
	if (nbytes < 0)
		return 0;

	return consumed + (size_t)nbytes;
}

void
UO::Server::OnSocketDisconnect() noexcept
{
	Log(2, "client closed the connection\n");
	handler.OnServerDisconnect();
}

void
UO::Server::OnSocketError(std::exception_ptr error) noexcept
{
	log_error("error during communication with client", std::move(error));
	handler.OnServerDisconnect();
}

void
UO::Server::Send(std::span<const std::byte> src)
{
	assert(!src.empty());
	assert(get_packet_length(protocol_version, src.data(), src.size()) == src.size());

	if (abort_event.IsPending())
		return;

	LogFmt(9, "sending packet to client, length={}\n", src.size());
	log_hexdump(10, src.data(), src.size());

	if (compression_enabled) {
		auto w = sock.Write();
		if (w.empty()) {
			Log(1, "output buffer full in uo_server_send()\n");
			Abort();
			return;
		}

		ssize_t nbytes = uo_compress((unsigned char *)w.data(), w.size(),
					     {(const unsigned char *)src.data(), src.size()});
		if (nbytes < 0) {
			Log(1, "uo_compress() failed\n");
			Abort();
			return;
		}

		sock.Append(static_cast<std::size_t>(nbytes));
	} else {
		if (!sock.Send(src)) {
			Log(1, "output buffer full in uo_server_send()\n");
			Abort();
		}
	}
}
