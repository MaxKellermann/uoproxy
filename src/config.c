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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef __GLIBC__
#include <getopt.h>
#endif
#include <netdb.h>

#include "config.h"
#include "netutil.h"

static void usage(void) {
    printf("usage: uoproxy [options] server:port\n\n"
           "valid options:\n"
           " -h             help (this text)\n"
#ifdef __GLIBC__
           " --port port\n"
#endif
           " -p port        listen on this port (default 2593)\n"
           );
}

/** read configuration options from the command line */
void parse_cmdline(struct config *config, int argc, char **argv) {
    int ret;
#ifdef __GLIBC__
    static const struct option long_options[] = {
        {"help", 0, 0, 'h'},
        {"port", 1, 0, 'p'},
        {0,0,0,0}
    };
#endif
    u_int16_t bind_port = 2593;
    const char *login_address;
    struct addrinfo hints;

    while (1) {
#ifdef __GLIBC__
        int option_index = 0;

        ret = getopt_long(argc, argv, "hp:",
                          long_options, &option_index);
#else
        ret = getopt(argc, argv, "hp:");
#endif
        if (ret == -1)
            break;

        switch (ret) {
        case 'h':
            usage();
            exit(0);

        case 'p':
            bind_port = (unsigned)strtoul(optarg, NULL, 10);
            if (bind_port == 0) {
                fprintf(stderr, "invalid port specification\n");
                exit(1);
            }
            break;

        default:
            exit(1);
        }
    }

    /* check non-option arguments */

    if (optind >= argc) {
        fprintf(stderr, "uoproxy: login server missing\n");
        fprintf(stderr, "Try 'uoproxy -h' for more information\n");
        exit(1);
    }

    login_address = argv[optind++];

    if (optind < argc) {
        fprintf(stderr, "uoproxy: unrecognized argument: %s\n",
                argv[optind + 1]);
        fprintf(stderr, "Try 'uoproxy -h' for more information\n");
        exit(1);
    }

    /* resolve login_address */

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;

    ret = getaddrinfo_helper(login_address, 2593, &hints,
                             &config->login_address);
    if (ret < 0) {
        fprintf(stderr, "failed to resolve '%s': %s\n",
                login_address, gai_strerror(ret));
        exit(1);
    }

    /* resolve bind_address */

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;

    ret = getaddrinfo_helper("*", bind_port, &hints,
                             &config->bind_address);
    if (ret < 0) {
        fprintf(stderr, "getaddrinfo_helper failed: %s\n",
                gai_strerror(ret));
        exit(1);
    }
}

void config_dispose(struct config *config) {
    if (config->bind_address != NULL) {
        freeaddrinfo(config->bind_address);
        config->bind_address = NULL;
    }

    if (config->login_address != NULL) {
        freeaddrinfo(config->login_address);
        config->login_address = NULL;
    }
}
