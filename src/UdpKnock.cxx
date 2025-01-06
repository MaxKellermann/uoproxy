// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "UdpKnock.hxx"
#include "uo/Packets.hxx"
#include "net/ConnectSocket.hxx"
#include "net/SocketAddress.hxx"
#include "net/SocketError.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/SpanCast.hxx"

void
SendUdpKnock(SocketAddress address,
	     const struct uo_packet_account_login &account_login)
{
	const auto s = CreateConnectDatagramSocket(address);

	if (s.Send(ReferenceAsBytes(account_login), MSG_DONTWAIT) < 0)
		throw MakeSocketError("Failed to send UDP knock");
}
