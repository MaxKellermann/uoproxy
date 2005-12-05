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
    printf("usage: uoproxy [options] [server:port]\n\n"
           "valid options:\n"
           " -h             help (this text)\n"
#ifdef __GLIBC__
           " --port port\n"
#endif
           " -p port        listen on this port (default 2593)\n"
           );
}

static struct addrinfo *port_to_addrinfo(unsigned port) {
    struct addrinfo hints, *ai;
    int ret;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;

    ret = getaddrinfo_helper("*", port, &hints,
                             &ai);
    if (ret != 0) {
        fprintf(stderr, "getaddrinfo_helper failed: %s\n",
                gai_strerror(ret));
        exit(2);
    }

    return ai;
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
    u_int16_t bind_port = 0;
    const char *login_address = NULL;
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

    if (optind < argc)
        login_address = argv[optind++];


    if (optind < argc) {
        fprintf(stderr, "uoproxy: unrecognized argument: %s\n",
                argv[optind + 1]);
        fprintf(stderr, "Try 'uoproxy -h' for more information\n");
        exit(1);
    }

    if (login_address == NULL && config->login_address == NULL) {
        fprintf(stderr, "uoproxy: login server missing\n");
        fprintf(stderr, "Try 'uoproxy -h' for more information\n");
        exit(1);
    }

    /* resolve login_address */

    if (login_address != NULL) {
        if (config->login_address != NULL)
            freeaddrinfo(config->login_address);

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
    }

    /* resolve bind_address */

    if (bind_port != 0) {
        if (config->bind_address != NULL)
            freeaddrinfo(config->bind_address);

        config->bind_address = port_to_addrinfo(bind_port);
    }
}

static const char *next_word(char **pp) {
    const char *word;

    while (**pp > 0 && **pp <= 0x20)
        ++(*pp);

    if (**pp == 0)
        return NULL;

    if (**pp == '"') {
        word = (*pp)++;
        while (**pp != 0 && **pp != '"')
            ++(*pp);
    } else {
        word = *pp;
        while (**pp < 0 || **pp > 0x20)
            ++(*pp);
    }

    if (**pp == 0)
        return word;

    **pp = 0;
    ++(*pp);

    return word;
}

static int parse_bool(const char *path, unsigned no, const char *val) {
    if (strcmp(val, "yes") == 0) {
        return 1;
    } else if (strcmp(val, "no") == 0) {
        return 0;
    } else {
        fprintf(stderr, "%s line %u: specify either 'yes' or 'no'\n",
                path, no);
        exit(2);
    }
}

int config_read_file(struct config *config, const char *path) {
    FILE *file;
    char line[2048], *p;
    const char *key, *value;
    unsigned no = 0;
    int ret;

    file = fopen(path, "r");
    if (file == NULL)
        return -errno;

    while (fgets(line, sizeof(line), file) != NULL) {
        /* increase line number */
        ++no;

        p = line;
        key = next_word(&p);
        if (key == NULL || *key == '#')
            continue;

        /* parse line */
        value = next_word(&p);
        if (value == NULL) {
            fprintf(stderr, "%s line %u: value missing after keyword\n",
                    path, no);
            exit(2);
        }

        if (next_word(&p) != NULL) {
            fprintf(stderr, "%s line %u: extra token after value\n",
                    path, no);
            exit(2);
        }

        /* check command */
        if (strcmp(key, "port") == 0) {
            unsigned long port = strtoul(value, NULL, 0);

            if (port == 0 || port > 0xffff) {
                fprintf(stderr, "%s line %u: invalid port\n",
                        path, no);
                exit(2);
            }

            if (config->bind_address != NULL)
                freeaddrinfo(config->bind_address);

            config->bind_address = port_to_addrinfo((unsigned)port);
        } else if (strcmp(key, "server") == 0) {
            struct addrinfo hints;

            if (config->login_address != NULL)
                freeaddrinfo(config->login_address);

            memset(&hints, 0, sizeof(hints));
            hints.ai_family = PF_INET;
            hints.ai_socktype = SOCK_STREAM;

            ret = getaddrinfo_helper(value, 2593, &hints,
                                     &config->login_address);
            if (ret < 0) {
                fprintf(stderr, "failed to resolve '%s': %s\n",
                        value, gai_strerror(ret));
                exit(1);
            }
        } else if (strcmp(key, "background") == 0) {
            config->background = parse_bool(path, no, value);
        } else if (strcmp(key, "autoreconnect") == 0) {
            config->autoreconnect = parse_bool(path, no, value);
        } else {
            fprintf(stderr, "%s line %u: invalid keyword '%s'\n",
                    path, no, key);
            exit(2);
        }
    }

    fclose(file);

    return 0;
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
