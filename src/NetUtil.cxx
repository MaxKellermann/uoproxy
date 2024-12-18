// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "NetUtil.hxx"
#include "net/SocketAddress.hxx"
#include "net/SocketError.hxx"
#include "net/UniqueSocketDescriptor.hxx"

#include <cassert>

UniqueSocketDescriptor
setup_server_socket(const SocketAddress bind_address)
{
    assert(bind_address != nullptr);

    UniqueSocketDescriptor s;
    if (!s.Create(bind_address.GetFamily(), SOCK_STREAM, 0))
        throw MakeSocketError("Failed to create socket");

    s.SetReuseAddress();

    if (!s.Bind(bind_address))
        throw MakeSocketError("Failed to bind");

    if (!s.Listen(4))
        throw MakeSocketError("listen() failed");

    return s;
}
