// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "ProxySocks.hxx"
#include "event/AwaitableSocketEvent.hxx"
#include "net/SocketAddress.hxx"
#include "net/SocketDescriptor.hxx"
#include "net/SocketError.hxx"
#include "co/Task.hxx"
#include "util/SpanCast.hxx"

#include <cstdint>
#include <stdexcept>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#endif

struct socks4_header {
	uint8_t version;
	uint8_t command;
	uint16_t port;
	uint32_t ip;
};

Co::Task<void>
ProxySocksConnect(EventLoop &event_loop, SocketDescriptor s, SocketAddress address)
{
	if (address.GetFamily() != AF_INET)
		throw std::invalid_argument{"Not an IPv4 address"};

	const struct sockaddr_in *in = (const struct sockaddr_in *)address.GetAddress();
	struct socks4_header header = {
		.version = 0x04,
		.command = 0x01,
		.port = in->sin_port,
		.ip = in->sin_addr.s_addr,
	};

	ssize_t nbytes = s.WriteNoWait(ReferenceAsBytes(header));
	if (nbytes < 0)
		throw MakeSocketError("Failed to send SOCKS4 request");

	if (nbytes != sizeof(header))
		throw std::runtime_error{"Failed to send SOCKS4 request"};

	static const char user[] = "";
	nbytes = s.WriteNoWait(ReferenceAsBytes(user));
	if (nbytes < 0)
		throw MakeSocketError("Failed to send SOCKS4 user");

	co_await AwaitableSocketEvent(event_loop, s, SocketEvent::READ);

	nbytes = s.ReadNoWait(ReferenceAsWritableBytes(header));
	if (nbytes < 0)
		throw MakeSocketError("Failed to receive SOCKS4 response");

	if (nbytes != sizeof(header))
		throw std::runtime_error{"Failed to receive SOCKS4 request"};

	if (header.command != 0x5a)
		throw std::runtime_error{"SOCKS4 request rejected"};
}
