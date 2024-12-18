// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <sys/types.h> /* for uid_t/gid_t */

struct game_server_config {
    char *name;
    struct addrinfo *address;
};

struct Config {
    struct addrinfo *bind_address = nullptr;

    /**
     * The address of the SOCKS4 proxy server.
     */
    struct addrinfo *socks4_address = nullptr;

    struct addrinfo *login_address = nullptr;

    unsigned num_game_servers = 0;
    struct game_server_config *game_servers = nullptr;
    bool background = false, autoreconnect = true, antispy = false, razor_workaround = false;

    /**
     * Always full light level?
     */
    bool light = false;

    char *client_version = nullptr;

    ~Config() noexcept;
};

/** read configuration options from the command line */
void parse_cmdline(Config *config, int argc, char **argv);

int config_read_file(Config *config, const char *path);
