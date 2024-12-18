// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

struct addrinfo;

int getaddrinfo_helper(const char *host_and_port, int default_port,
                       const struct addrinfo *hints,
                       struct addrinfo **aip);

int setup_server_socket(const struct addrinfo *bind_address);
