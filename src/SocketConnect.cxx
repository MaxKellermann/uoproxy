// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "SocketConnect.hxx"
#include "net/SocketAddress.hxx"
#include "net/SocketError.hxx"
#include "net/UniqueSocketDescriptor.hxx"

UniqueSocketDescriptor
socket_connect(int domain, int type, int protocol,
	       SocketAddress address)
{
	UniqueSocketDescriptor s;
	if (!s.Create(domain, type, protocol))
		throw MakeSocketError("Failed to create socket");

	if (!s.Connect(address))
		throw MakeSocketError("Failed to connect");

	return s;
}
