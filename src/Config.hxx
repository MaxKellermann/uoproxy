/*
 * uoproxy
 *
 * Copyright 2005-2020 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef __CONFIG_H
#define __CONFIG_H

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

#endif
