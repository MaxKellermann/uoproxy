// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Config.hxx"
#include "NetUtil.hxx"
#include "version.h"
#include "Log.hxx"

#include <fmt/core.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef __GLIBC__
#include <getopt.h>
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <getopt.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#endif

static void
usage()
{
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
        fmt::print(stderr, "getaddrinfo_helper failed: {}\n",
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
        fmt::print(stderr, "getaddrinfo_helper failed: {}\n",
                   gai_strerror(ret));
        exit(2);
    }

    return ai;
}

/** read configuration options from the command line */
void parse_cmdline(Config *config, int argc, char **argv) {
    int ret;
#ifdef __GLIBC__
    static const struct option long_options[] = {
        {"help", 0, 0, 'h'},
        {"version", 0, 0, 'V'},
        {"verbose", 0, 0, 'v'},
        {"quiet", 1, 0, 'q'},
        {"port", 1, 0, 'p'},
        {"bind", 1, 0, 'b'},
        {0,0,0,0}
    };
#endif
    int bind_port = 0;
    const char *bind_address = nullptr, *login_address = nullptr;
    struct addrinfo hints;

    while (1) {
#ifdef __GLIBC__
        int option_index = 0;

        ret = getopt_long(argc, argv, "hVvqp:D",
                          long_options, &option_index);
#else
        ret = getopt(argc, argv, "hVvqp:D");
#endif
        if (ret == -1)
            break;

        switch (ret) {
        case 'h':
            usage();
            exit(0);

        case 'V':
            printf("uoproxy v" VERSION
                   ", https://github.com/MaxKellermann/uoproxy\n");
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
                fmt::print(stderr, "invalid port specification\n");
                exit(1);
            }
            break;

        case 'b':
            bind_address = optarg;
            break;

        case 'D':
            /* obsolete, ignore for compatibility */
            break;

        default:
            exit(1);
        }
    }

    /* check non-option arguments */

    if (optind < argc)
        login_address = argv[optind++];


    if (optind < argc) {
        fmt::print(stderr, "uoproxy: unrecognized argument: {:?}\n",
                   argv[optind]);
        fmt::print(stderr, "Try 'uoproxy -h' for more information\n");
        exit(1);
    }

    if (login_address == nullptr && config->login_address == nullptr &&
        config->num_game_servers == 0) {
        fmt::print(stderr, "uoproxy: login server missing\n");
        fmt::print(stderr, "Try 'uoproxy -h' for more information\n");
        exit(1);
    }

    /* resolve login_address */

    if (login_address != nullptr) {
        if (config->login_address != nullptr)
            freeaddrinfo(config->login_address);

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = PF_INET;
        hints.ai_socktype = SOCK_STREAM;

        ret = getaddrinfo_helper(login_address, 2593, &hints,
                                 &config->login_address);
        if (ret < 0) {
            fmt::print(stderr, "failed to resolve {:?}: {}\n",
                    login_address, gai_strerror(ret));
            exit(1);
        }
    }

    /* resolve bind_address */

    if (bind_address != nullptr) {
        if (bind_port != 0) {
            fmt::print(stderr, "You cannot specifiy both "
#ifdef __GLIBC__
                       "--bind and --port"
#else
                       "-b and -p"
#endif
                       "\n");
            exit(1);
        }

        if (config->bind_address != nullptr)
            freeaddrinfo(config->bind_address);

        config->bind_address = parse_address(bind_address);
    }

    if (bind_port == 0 && config->bind_address == nullptr)
        bind_port = 2593;

    if (bind_port != 0) {
        if (config->bind_address != nullptr)
            freeaddrinfo(config->bind_address);

        config->bind_address = port_to_addrinfo(bind_port);
    }
}

static char *next_word(char **pp) {
    char *word;

    while (**pp > 0 && **pp <= 0x20)
        ++(*pp);

    if (**pp == 0)
        return nullptr;

    if (**pp == '"') {
        word = ++(*pp);
        while (**pp != 0 && **pp != '"')
            ++(*pp);
    } else {
        word = *pp;
        while ((unsigned char)**pp > 0x20)
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
        fmt::print(stderr, "{} line {}: specify either 'yes' or 'no'\n",
                   path, no);
        exit(2);
    }
}

static void assign_string(char **destp, const char *src) {
    if (*destp != nullptr)
        free(*destp);

    *destp = *src == 0 ? nullptr : strdup(src);
}

static void
parse_game_server(const char *path, unsigned no,
                  struct game_server_config *config, char *string)
{
    char *eq = strchr(string, '=');
    struct addrinfo hints;
    int ret;

    if (eq == nullptr) {
        fmt::print(stderr, "{} line {}: no address for server ('=' missing)\n",
                   path, no);
        exit(2);
    }

    *eq = 0;

    config->name = strdup(string);
    if (config->name == nullptr) {
        log_oom();
        exit(2);
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;

    ret = getaddrinfo_helper(eq + 1, 2593, &hints, &config->address);
    if (ret < 0) {
        fmt::print(stderr, "failed to resolve {:?}: {}\n",
                   eq + 1, gai_strerror(ret));
        exit(1);
    }
}

int config_read_file(Config *config, const char *path) {
    FILE *file;
    char line[2048], *p;
    char *key, *value;
    unsigned no = 0;
    int ret;

    file = fopen(path, "r");
    if (file == nullptr)
        return errno;

    while (fgets(line, sizeof(line), file) != nullptr) {
        /* increase line number */
        ++no;

        p = line;
        key = next_word(&p);
        if (key == nullptr || *key == '#')
            continue;

        /* parse line */
        value = next_word(&p);
        if (value == nullptr) {
            fmt::print(stderr, "{} line {}: value missing after keyword\n",
                       path, no);
            exit(2);
        }

        if (next_word(&p) != nullptr) {
            fmt::print(stderr, "{} line {}: extra token after value\n",
                       path, no);
            exit(2);
        }

        /* check command */
        if (strcmp(key, "port") == 0) {
            unsigned long port = strtoul(value, nullptr, 0);

            if (port == 0 || port > 0xffff) {
                fmt::print(stderr, "{} line {}: invalid port\n",
                           path, no);
                exit(2);
            }

            if (config->bind_address != nullptr)
                freeaddrinfo(config->bind_address);

            config->bind_address = port_to_addrinfo((unsigned)port);
        } else if (strcmp(key, "bind") == 0) {
            if (config->bind_address != nullptr)
                freeaddrinfo(config->bind_address);

            config->bind_address = parse_address(value);
        } else if (strcmp(key, "socks4") == 0) {
            struct addrinfo hints;

            if (config->socks4_address != nullptr)
                freeaddrinfo(config->socks4_address);

            memset(&hints, 0, sizeof(hints));
            hints.ai_family = PF_INET;
            hints.ai_socktype = SOCK_STREAM;

            ret = getaddrinfo_helper(value, 9050, &hints,
                                     &config->socks4_address);
            if (ret < 0) {
                fmt::print(stderr, "failed to resolve {:?}: {}\n",
                           value, gai_strerror(ret));
                exit(1);
            }
        } else if (strcmp(key, "server") == 0) {
            struct addrinfo hints;

            if (config->login_address != nullptr)
                freeaddrinfo(config->login_address);

            memset(&hints, 0, sizeof(hints));
            hints.ai_family = PF_INET;
            hints.ai_socktype = SOCK_STREAM;

            ret = getaddrinfo_helper(value, 2593, &hints,
                                     &config->login_address);
            if (ret < 0) {
                fmt::print(stderr, "failed to resolve {:?}: {}\n",
                           value, gai_strerror(ret));
                exit(1);
            }
        } else if (strcmp(key, "server_list") == 0) {
            unsigned i;

            if (config->game_servers != nullptr) {
                for (i = 0; i < config->num_game_servers; i++) {
                    if (config->game_servers[i].name != nullptr)
                        free(config->game_servers[i].name);
                    if (config->game_servers[i].address != nullptr)
                        freeaddrinfo(config->game_servers[i].address);
                }

                config->game_servers = nullptr;
            }

            config->num_game_servers = 0;

            if (*value == 0)
                continue;

            ++config->num_game_servers;

            for (p = value; (p = strchr(p, ',')) != nullptr; ++p)
                ++config->num_game_servers;

            config->game_servers = (struct game_server_config *)calloc(config->num_game_servers,
                                          sizeof(*config->game_servers));
            if (config->game_servers == nullptr) {
                log_oom();
                exit(2);
            }

            for (p = value, i = 0; i < config->num_game_servers; ++i) {
                char *o = p;

                p = strchr(o, ',');
                if (p == nullptr) {
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
            fmt::print(stderr, "{} line {}: invalid keyword {:?}\n",
                       path, no, key);
            exit(2);
        }
    }

    fclose(file);

    return 0;
}

Config::~Config() noexcept
{
    if (bind_address != nullptr)
        freeaddrinfo(bind_address);

    if (socks4_address != nullptr)
        freeaddrinfo(socks4_address);

    if (login_address != nullptr)
        freeaddrinfo(login_address);

    if (game_servers != nullptr) {
        unsigned i;
        for (i = 0; i < num_game_servers; i++) {
            free(game_servers[i].name);
            if (game_servers[i].address != nullptr)
                freeaddrinfo(game_servers[i].address);
        }
        free(game_servers);
    }

    free(client_version);
}
