// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <string>
#include <vector>

struct game_server_config {
    std::string name;
    struct addrinfo *address;
};

struct Config {
    struct addrinfo *bind_address = nullptr;

    /**
     * The address of the SOCKS4 proxy server.
     */
    struct addrinfo *socks4_address = nullptr;

    struct addrinfo *login_address = nullptr;

    std::vector<struct game_server_config> game_servers;
    bool background = false, autoreconnect = true, antispy = false, razor_workaround = false;

    /**
     * Always full light level?
     */
    bool light = false;

    std::string client_version;

    ~Config() noexcept;
};

/** read configuration options from the command line */
void parse_cmdline(Config *config, int argc, char **argv);

int config_read_file(Config *config, const char *path);
