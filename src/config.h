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

#ifndef __CONFIG_H
#define __CONFIG_H

#include <stdbool.h>
#include <sys/types.h> /* for uid_t/gid_t */

#ifdef WIN32
#define DISABLE_DAEMON_CODE
#endif

struct game_server_config {
    char *name;
    struct addrinfo *address;
};

struct config {
    struct addrinfo *bind_address;

    /**
     * The address of the SOCKS4 proxy server.
     */
    struct addrinfo *socks4_address;

    struct addrinfo *login_address;

    unsigned num_game_servers;
    struct game_server_config *game_servers;
    bool background, autoreconnect, antispy, razor_workaround;

    /**
     * Always full light level?
     */
    bool light;

    char *client_version;
    /* daemon config */
#ifndef DISABLE_DAEMON_CODE
    int no_daemon;
    char *pidfile, *logger, *chroot_dir;
    uid_t uid;
    gid_t gid;
#endif
};

/** read configuration options from the command line */
void parse_cmdline(struct config *config, int argc, char **argv);

int config_read_file(struct config *config, const char *path);

void config_dispose(struct config *config);

#endif
