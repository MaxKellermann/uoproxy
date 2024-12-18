// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "NetUtil.hxx"
#include "net/SocketAddress.hxx"
#include "net/SocketError.hxx"
#include "net/UniqueSocketDescriptor.hxx"

#include <cassert>

#include <errno.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#endif

int getaddrinfo_helper(const char *host_and_port, int default_port,
                       const struct addrinfo *hints,
                       struct addrinfo **aip) {
    const char *colon, *host, *port;
    char buffer[256];

    colon = strchr(host_and_port, ':');
    if (colon == nullptr) {
        snprintf(buffer, sizeof(buffer), "%d", default_port);

        host = host_and_port;
        port = buffer;
    } else {
        size_t len = colon - host_and_port;

        if (len >= sizeof(buffer)) {
            errno = ENAMETOOLONG;
#ifdef _WIN32
            return EAI_FAIL;
#else
            return EAI_SYSTEM;
#endif
        }

        memcpy(buffer, host_and_port, len);
        buffer[len] = 0;

        host = buffer;
        port = colon + 1;
    }

    if (strcmp(host, "*") == 0)
        host = "0.0.0.0";

    return getaddrinfo(host, port, hints, aip);
}

UniqueSocketDescriptor
setup_server_socket(const struct addrinfo *bind_address)
{
    assert(bind_address != nullptr);

    UniqueSocketDescriptor s;
    if (!s.Create(bind_address->ai_family, SOCK_STREAM, 0))
        throw MakeSocketError("Failed to create socket");

    s.SetReuseAddress();

    if (!s.Bind({bind_address->ai_addr, bind_address->ai_addrlen}))
        throw MakeSocketError("Failed to bind");

    if (!s.Listen(4))
        throw MakeSocketError("listen() failed");

    return s;
}
