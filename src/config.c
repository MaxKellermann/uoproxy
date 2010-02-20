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

#include "config.h"
#include "netutil.h"
#include "version.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef __GLIBC__
#include <getopt.h>
#endif
#ifndef DISABLE_DAEMON_CODE
#include <sys/stat.h>
#include <pwd.h>
#include <unistd.h>
#endif

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <getopt.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#endif

static void usage(void) {
    puts("usage: uoproxy [options] [server:port]\n\n"
         "valid options:\n"
         " -h             help (this text)\n"
#ifdef __GLIBC__
         " --version\n"
#endif
         " -V             show uoproxy version\n"
#ifdef __GLIBC__
         " --verbose\n"
#endif
         " -v             be more verbose\n"
#ifdef __GLIBC__
         " --quiet\n"
#endif
         " -q             be quiet\n"
#ifdef __GLIBC__
         " --port port\n"
#endif
         " -p port        listen on this port (default 2593)\n"
#ifdef __GLIBC__
         " --bind IP:port\n"
#endif
         " -b IP:port     listen on this IP and port (default *:2593)\n"
#ifndef DISABLE_DAEMON_CODE
#ifdef __GLIBC__
         " --logger program\n"
#endif
         " -l program     specifies a logger program (executed by /bin/sh)\n"
#ifdef __GLIBC__
         " --chroot dir\n"
#endif
         " -r dir         chroot into this directory (requires root)\n"
#ifdef __GLIBC__
         " --user username\n"
#endif
         " -u username    change user id (don't run uoamhub as root!)\n"
         " -D             don't detach (daemonize)\n"
#ifdef __GLIBC__
         " --pidfile file\n"
#endif
         " -P file        create a pid file\n"
#endif
         "\n"
         );
}

static struct addrinfo *port_to_addrinfo(int port) {
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

static struct addrinfo *
parse_address(const char *host_and_port)
{
    struct addrinfo hints, *ai;
    int ret;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;

    ret = getaddrinfo_helper(host_and_port, 2593, &hints, &ai);
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
        {"version", 0, 0, 'V'},
        {"verbose", 0, 0, 'v'},
        {"quiet", 1, 0, 'q'},
        {"port", 1, 0, 'p'},
        {"bind", 1, 0, 'b'},
#ifndef DISABLE_DAEMON_CODE
        {"chroot", 1, 0, 'r'},
        {"user", 1, 0, 'u'},
        {"logger", 1, 0, 'l'},
        {"pidfile", 1, 0, 'P'},
#endif
        {0,0,0,0}
    };
#endif
    int bind_port = 0;
    const char *bind_address = NULL, *login_address = NULL;
    struct addrinfo hints;
#ifndef DISABLE_DAEMON_CODE
    struct passwd *pw;
    struct stat st;
#endif

    while (1) {
#ifdef __GLIBC__
        int option_index = 0;

        ret = getopt_long(argc, argv, "hVvqp:DP:l:r:u:",
                          long_options, &option_index);
#else
        ret = getopt(argc, argv, "hVvqp:DP:l:r:u:");
#endif
        if (ret == -1)
            break;

        switch (ret) {
        case 'h':
            usage();
            exit(0);

        case 'V':
            printf("uoproxy v" VERSION
                   ", http://max.kellermann.name/projects/uoproxy/\n");
            exit(0);

#ifndef DISABLE_LOGGING
        case 'v':
            ++verbose;
            break;

        case 'q':
            verbose = 0;
            break;
#endif

        case 'p':
            bind_port = atoi(optarg);
            if (bind_port == 0) {
                fprintf(stderr, "invalid port specification\n");
                exit(1);
            }
            break;

        case 'b':
            bind_address = optarg;
            break;

#ifndef DISABLE_DAEMON_CODE
        case 'D':
            config->no_daemon = 1;
            break;

        case 'P':
            if (config->pidfile != NULL)
                free(config->pidfile);
            config->pidfile = strdup(optarg);
            break;

        case 'l':
            if (config->logger != NULL)
                free(config->logger);
            config->logger = strdup(optarg);
            break;

        case 'r':
            ret = stat(optarg, &st);
            if (ret < 0) {
                fprintf(stderr, "failed to stat '%s': %s\n",
                        optarg, strerror(errno));
                exit(1);
            }
            if (!S_ISDIR(st.st_mode)) {
                fprintf(stderr, "not a directory: '%s'\n",
                        optarg);
                exit(1);
            }

            if (config->chroot_dir != NULL)
                free(config->chroot_dir);
            config->chroot_dir = strdup(optarg);
            break;

        case 'u':
            pw = getpwnam(optarg);
            if (pw == NULL) {
                fprintf(stderr, "user '%s' not found\n", optarg);
                exit(1);
            }
            if (pw->pw_uid == 0) {
                fprintf(stderr, "setuid root is not allowed\n");
                exit(1);
            }
            config->uid = pw->pw_uid;
            config->gid = pw->pw_gid;
            break;
#endif

        default:
            exit(1);
        }
    }

    /* check non-option arguments */

    if (optind < argc)
        login_address = argv[optind++];


    if (optind < argc) {
        fprintf(stderr, "uoproxy: unrecognized argument: %s\n",
                argv[optind]);
        fprintf(stderr, "Try 'uoproxy -h' for more information\n");
        exit(1);
    }

    if (login_address == NULL && config->login_address == NULL &&
        config->num_game_servers == 0) {
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

    if (bind_address != NULL) {
        if (bind_port != 0) {
            fprintf(stderr, "You cannot specifiy both "
#ifdef __GLIBC__
                    "--bind and --port"
#else
                    "-b and -p"
#endif
                    "\n");
            exit(1);
        }

        if (config->bind_address != NULL)
            freeaddrinfo(config->bind_address);

        config->bind_address = parse_address(bind_address);
    }

    if (bind_port == 0 && config->bind_address == NULL)
        bind_port = 2593;

    if (bind_port != 0) {
        if (config->bind_address != NULL)
            freeaddrinfo(config->bind_address);

        config->bind_address = port_to_addrinfo(bind_port);
    }
}

static char *next_word(char **pp) {
    char *word;

    while (**pp > 0 && **pp <= 0x20)
        ++(*pp);

    if (**pp == 0)
        return NULL;

    if (**pp == '"') {
        word = ++(*pp);
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

static bool
parse_bool(const char *path, unsigned no, const char *val) {
    if (strcmp(val, "yes") == 0) {
        return true;
    } else if (strcmp(val, "no") == 0) {
        return false;
    } else {
        fprintf(stderr, "%s line %u: specify either 'yes' or 'no'\n",
                path, no);
        exit(2);
    }
}

static void assign_string(char **destp, const char *src) {
    if (*destp != NULL)
        free(*destp);

    *destp = *src == 0 ? NULL : strdup(src);
}

static void
parse_game_server(const char *path, unsigned no,
                  struct game_server_config *config, char *string)
{
    char *eq = strchr(string, '=');
    struct addrinfo hints;
    int ret;

    if (eq == NULL) {
        fprintf(stderr, "%s line %u: no address for server ('=' missing)\n",
                path, no);
        exit(2);
    }

    *eq = 0;

    config->name = strdup(string);
    if (config->name == NULL) {
        log_oom();
        exit(2);
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;

    ret = getaddrinfo_helper(eq + 1, 2593, &hints, &config->address);
    if (ret < 0) {
        fprintf(stderr, "failed to resolve '%s': %s\n",
                eq + 1, gai_strerror(ret));
        exit(1);
    }
}

int config_read_file(struct config *config, const char *path) {
    FILE *file;
    char line[2048], *p;
    char *key, *value;
    unsigned no = 0;
    int ret;

    file = fopen(path, "r");
    if (file == NULL)
        return errno;

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
        } else if (strcmp(key, "bind") == 0) {
            if (config->bind_address != NULL)
                freeaddrinfo(config->bind_address);

            config->bind_address = parse_address(value);
        } else if (strcmp(key, "socks4") == 0) {
            struct addrinfo hints;

            if (config->socks4_address != NULL)
                freeaddrinfo(config->socks4_address);

            memset(&hints, 0, sizeof(hints));
            hints.ai_family = PF_INET;
            hints.ai_socktype = SOCK_STREAM;

            ret = getaddrinfo_helper(value, 9050, &hints,
                                     &config->socks4_address);
            if (ret < 0) {
                fprintf(stderr, "failed to resolve '%s': %s\n",
                        value, gai_strerror(ret));
                exit(1);
            }
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
        } else if (strcmp(key, "pidfile") == 0) {
#ifndef DISABLE_DAEMON_CODE
            assign_string(&config->pidfile, value);
#endif
        } else if (strcmp(key, "logger") == 0) {
#ifndef DISABLE_DAEMON_CODE
            assign_string(&config->logger, value);
#endif
        } else if (strcmp(key, "chroot") == 0) {
#ifndef DISABLE_DAEMON_CODE
            assign_string(&config->chroot_dir, value);
#endif
        } else if (strcmp(key, "user") == 0) {
#ifndef DISABLE_DAEMON_CODE
            struct passwd *pw;

            pw = getpwnam(value);
            if (pw == NULL) {
                fprintf(stderr, "%s line %u: user '%s' not found\n",
                        path, no, value);
                exit(2);
            }

            if (pw->pw_uid == 0) {
                fprintf(stderr, "%s line %u: setuid root is not allowed\n",
                        path, no);
                exit(1);
            }

            config->uid = pw->pw_uid;
            config->gid = pw->pw_gid;
#endif
        } else if (strcmp(key, "server_list") == 0) {
            unsigned i;

            if (config->game_servers != NULL) {
                for (i = 0; i < config->num_game_servers; i++) {
                    if (config->game_servers[i].name != NULL)
                        free(config->game_servers[i].name);
                    if (config->game_servers[i].address != NULL)
                        freeaddrinfo(config->game_servers[i].address);
                }

                config->game_servers = NULL;
            }

            config->num_game_servers = 0;

            if (*value == 0)
                continue;

            ++config->num_game_servers;

            for (p = value; (p = strchr(p, ',')) != NULL; ++p)
                ++config->num_game_servers;

            config->game_servers = calloc(config->num_game_servers,
                                          sizeof(*config->game_servers));
            if (config->game_servers == NULL) {
                log_oom();
                exit(2);
            }

            for (p = value, i = 0; i < config->num_game_servers; ++i) {
                char *o = p;

                p = strchr(o, ',');
                if (p == NULL) {
                    parse_game_server(path, no,
                                      config->game_servers + i, o);
                    break;
                } else {
                    *p++ = 0;
                    parse_game_server(path, no,
                                      config->game_servers + i, o);
                }
            }
        } else if (strcmp(key, "background") == 0) {
            config->background = parse_bool(path, no, value);
        } else if (strcmp(key, "autoreconnect") == 0) {
            config->autoreconnect = parse_bool(path, no, value);
        } else if (strcmp(key, "antispy") == 0) {
            config->antispy = parse_bool(path, no, value);
        } else if (strcmp(key, "razor_workaround") == 0) {
            config->razor_workaround = parse_bool(path, no, value);
        } else if (strcmp(key, "light") == 0) {
            config->light = parse_bool(path, no, value);
        } else if (strcmp(key, "client_version") == 0) {
            assign_string(&config->client_version, value);
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

    if (config->socks4_address != NULL) {
        freeaddrinfo(config->socks4_address);
        config->socks4_address = NULL;
    }

    if (config->login_address != NULL) {
        freeaddrinfo(config->login_address);
        config->login_address = NULL;
    }

    if (config->game_servers != NULL) {
        unsigned i;
        for (i = 0; i < config->num_game_servers; i++) {
            if (config->game_servers[i].name != NULL)
                free(config->game_servers[i].name);
            if (config->game_servers[i].address != NULL)
                freeaddrinfo(config->game_servers[i].address);
        }
        free(config->game_servers);
    }

    if (config->client_version != NULL) {
        free(config->client_version);
        config->client_version = NULL;
    }

#ifndef DISABLE_DAEMON_CODE
    if (config->pidfile != NULL)
        free(config->pidfile);

    if (config->logger != NULL)
        free(config->logger);

    if (config->chroot_dir != NULL)
        free(config->chroot_dir);
#endif
}
