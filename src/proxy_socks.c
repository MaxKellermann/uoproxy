/*
 * uoproxy
 *
 * (c) 2005-2010 Max Kellermann <max@duempel.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "proxy_socks.h"
#include "log.h"

#include <stdint.h>
#include <sys/types.h>

#ifdef WIN32
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
socks_connect(int fd, const struct sockaddr *address)
{
    if (address->sa_family != AF_INET) {
        log(1, "Not an IPv4 address\n");
        return false;
    }

    const struct sockaddr_in *in = (const struct sockaddr_in *)address;
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
        log(1, "Failed to send SOCKS4 request\n");
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
        log(1, "Failed to receive SOCKS4 response\n");
        return false;
    }

    if (header.command != 0x5a) {
        log(2, "SOCKS4 request rejected: 0x%02x\n", header.command);
        return false;
    }

    return true;
}
