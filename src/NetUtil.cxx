// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "NetUtil.hxx"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
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

int setup_server_socket(const struct addrinfo *bind_address) {
    int sockfd, ret;

    assert(bind_address != nullptr);

    sockfd = socket(bind_address->ai_family, SOCK_STREAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "failed to create socket: %s\n",
                strerror(errno));
        exit(1);
    }

#ifndef _WIN32
    int param = 1;
    ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &param, sizeof(param));
    if (ret < 0) {
        fprintf(stderr, "setsockopt failed: %s\n",
                strerror(errno));
        exit(1);
    }
#endif

    ret = bind(sockfd, bind_address->ai_addr,
               bind_address->ai_addrlen);
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
