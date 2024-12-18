// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "net/AddressInfo.hxx"

#include <string>
#include <vector>

struct game_server_config {
    const std::string name;
    AddressInfoList address;

    [[nodiscard]]
    explicit game_server_config(const char *_name) noexcept
        :name(_name) {}
};

struct Config {
    AddressInfoList bind_address;

    /**
     * The address of the SOCKS4 proxy server.
     */
    AddressInfoList socks4_address;

    AddressInfoList login_address;

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
