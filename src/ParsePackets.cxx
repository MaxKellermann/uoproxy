// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "ParsePackets.hxx"
#include "PacketHandler.hxx"
#include "Log.hxx"
#include "uo/Length.hxx"
#include "event/net/BufferedSocket.hxx"
#include "net/SocketProtocolError.hxx"

namespace UO {

BufferedResult
ParsePackets(DefaultFifoBuffer &buffer, ProtocolVersion protocol_version,
	     std::string_view peer_name,
	     PacketHandler &handler)
{
	while (true) {
		const auto src = buffer.Read();
		if (src.empty()) {
			buffer.Free();
			return BufferedResult::OK;
		}

		const std::size_t packet_length = GetPacketLength(src, protocol_version);
		if (packet_length == PACKET_LENGTH_INVALID) {
			LogFmt(1, "malformed packet from {}\n", peer_name);
			log_hexdump(5, src);
			throw SocketProtocolError{fmt::format("Malformed {:#02x} packet", src.front())};
		}

		LogFmt(9, "from {}: {:#02x} length={}\n",
		       peer_name, src.front(), packet_length);

		if (packet_length == 0 || packet_length > src.size())
			return BufferedResult::MORE;

		const auto packet = src.first(packet_length);

		log_hexdump(10, packet);

		switch (handler.OnPacket(packet)) {
		case PacketHandler::OnPacketResult::OK:
			break;

		case PacketHandler::OnPacketResult::BLOCKING:
			return BufferedResult::OK;

		case PacketHandler::OnPacketResult::CLOSED:
			return BufferedResult::DESTROYED;
		}

		buffer.Consume(packet_length);
	}
}

} // namespace UO
