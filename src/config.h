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

#ifndef __CONFIG_H
#define __CONFIG_H

struct config {
    struct addrinfo *bind_address, *login_address;
    int background, autoreconnect;
    char *client_version;
};

/** read configuration options from the command line */
void parse_cmdline(struct config *config, int argc, char **argv);

int config_read_file(struct config *config, const char *path);

void config_dispose(struct config *config);

#endif
