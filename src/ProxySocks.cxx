// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "ProxySocks.hxx"
#include "Log.hxx"
#include "net/SocketAddress.hxx"

#include <stdint.h>
#include <sys/types.h>

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

bool
socks_connect(int fd, SocketAddress address)
{
    if (address.GetFamily() != AF_INET) {
        LogFormat(1, "Not an IPv4 address\n");
        return false;
    }

    const struct sockaddr_in *in = (const struct sockaddr_in *)address.GetAddress();
    struct socks4_header header = {
        .version = 0x04,
        .command = 0x01,
        .port = in->sin_port,
        .ip = in->sin_addr.s_addr,
    };

    ssize_t nbytes = send(fd, (const char *)&header, sizeof(header), 0);
    if (nbytes < 0) {
        log_errno("Failed to send SOCKS4 request");
        return false;
    }

    if (nbytes != sizeof(header)) {
        LogFormat(1, "Failed to send SOCKS4 request\n");
        return false;
    }

    static const char user[] = "";
    nbytes = send(fd, user, sizeof(user), 0);
    if (nbytes < 0) {
        log_errno("Failed to send SOCKS4 user");
        return false;
    }

    nbytes = recv(fd, (char *)&header, sizeof(header), 0);
    if (nbytes < 0) {
        log_errno("Failed to receive SOCKS4 response");
        return false;
    }

    if (nbytes != sizeof(header)) {
        LogFormat(1, "Failed to receive SOCKS4 response\n");
        return false;
    }

    if (header.command != 0x5a) {
        LogFormat(2, "SOCKS4 request rejected: 0x%02x\n", header.command);
        return false;
    }

    return true;
}
