/*
 * uoproxy
 * $Id$
 *
 * (c) 2005 Max Kellermann <max@duempel.org>
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

#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "netutil.h"

int getaddrinfo_helper(const char *host_and_port, int default_port,
                       const struct addrinfo *hints,
                       struct addrinfo **aip) {
    const char *colon, *host, *port;
    char buffer[256];

    colon = strchr(host_and_port, ':');
    if (colon == NULL) {
        snprintf(buffer, sizeof(buffer), "%d", default_port);

        host = host_and_port;
        port = buffer;
    } else {
        size_t len = colon - host_and_port;

        if (len >= sizeof(buffer)) {
            errno = ENAMETOOLONG;
            return EAI_SYSTEM;
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

int setup_server_socket(uint32_t ip, uint16_t port) {
    int sockfd, ret, param;
    struct sockaddr_in sin;

    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "failed to create socket: %s\n",
                strerror(errno));
        exit(1);
    }

    param = 1;
    ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &param, sizeof(param));
    if (ret < 0) {
        fprintf(stderr, "setsockopt failed: %s\n",
                strerror(errno));
        exit(1);
    }

    sin.sin_family = AF_INET;
    sin.sin_port = port;
    sin.sin_addr.s_addr = ip;

    ret = bind(sockfd, (const struct sockaddr*)&sin, sizeof(sin));
    if (ret < 0) {
        fprintf(stderr, "failed to bind: %s\n",
                strerror(errno));
        exit(1);
    }

    ret = listen(sockfd, 4);
    if (ret < 0) {
        fprintf(stderr, "listen failed: %s\n",
                strerror(errno));
        exit(1);
    }

    return sockfd;
}

int setup_client_socket(uint32_t ip, uint16_t port) {
    int sockfd, ret;
    struct sockaddr_in sin;

    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "failed to create socket: %s\n",
                strerror(errno));
        exit(1);
    }

    sin.sin_family = AF_INET;
    sin.sin_port = port;
    sin.sin_addr.s_addr = ip;

    ret = connect(sockfd, (struct sockaddr*)&sin, sizeof(sin));
    if (ret < 0) {
        fprintf(stderr, "connect failed: %s\n",
                strerror(errno));
        exit(1);
    }

    return sockfd;
}

int socket_connect(uint32_t ip, uint16_t port) {
    int sockfd, ret;
    struct sockaddr_in sin;

    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        return -1;

    sin.sin_family = AF_INET;
    sin.sin_port = port;
    sin.sin_addr.s_addr = ip;

    ret = connect(sockfd, (struct sockaddr*)&sin, sizeof(sin));
    if (ret < 0)
        return -1;

    return sockfd;
}
