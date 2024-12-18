// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Instance.hxx"
#include "Connection.hxx"
#include "Config.hxx"
#include "version.h"
#include "Log.hxx"
#include "config.h"

#ifdef HAVE_LIBSYSTEMD
#include <systemd/sd-daemon.h>
#endif

#include <exception>

#ifndef _WIN32
#include <sys/signal.h>
#include <signal.h>
#endif

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#endif

#ifndef _WIN32

static void
deinit_signals(Instance *instance)
{
    (void)instance;
    event_del(&instance->sigterm_event);
    event_del(&instance->sigint_event);
    event_del(&instance->sigquit_event);
}

static void
exit_event_callback(int, short, void *ctx)
{
    Instance *instance = (Instance *)ctx;

    if (instance->should_exit)
        return;

    instance->should_exit = true;

    deinit_signals(instance);

    if (instance->server_socket >= 0) {
        event_del(&instance->server_socket_event);
        close(instance->server_socket);
        instance->server_socket = -1;
    }

    instance->connections.clear_and_dispose([](Connection *c) {
        delete c;
    });
}

#endif

static void config_get(Config *config, int argc, char **argv) {
    const char *home;
    char path[4096];
    int ret;

    home = getenv("HOME");
    if (home == nullptr) {
        ret = 1;
    } else {
        snprintf(path, sizeof(path), "%s/.uoproxyrc", home);
        ret = config_read_file(config, path);
    }

    if (ret != 0)
        config_read_file(config, "/etc/uoproxy.conf");

    parse_cmdline(config, argc, argv);
}

static void
setup_signal_handlers(Instance *instance)
{
#ifdef _WIN32
    (void)instance;
#else
    signal(SIGPIPE, SIG_IGN);

    event_set(&instance->sigterm_event, SIGTERM, EV_SIGNAL|EV_PERSIST,
              exit_event_callback, instance);
    event_add(&instance->sigterm_event, nullptr);

    event_set(&instance->sigint_event, SIGINT, EV_SIGNAL|EV_PERSIST,
              exit_event_callback, instance);
    event_add(&instance->sigint_event, nullptr);

    event_set(&instance->sigquit_event, SIGQUIT, EV_SIGNAL|EV_PERSIST,
              exit_event_callback, instance);
    event_add(&instance->sigquit_event, nullptr);
#endif
}

int main(int argc, char **argv)
try {
    Config config;
    Instance instance(config);

    /* WinSock */

#ifdef _WIN32
    WSADATA wsaData;

    if ((WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0 ||
        LOBYTE(wsaData.wVersion) != 2 ||
        HIBYTE(wsaData.wVersion) != 2 ) {
        fprintf(stderr, "WSAStartup() failed\n");
        return 1;
    }
#endif

    /* configuration */

    config_get(&config, argc, argv);

    LogFormat(1, "uoproxy v" VERSION
        ", https://github.com/MaxKellermann/uoproxy\n");

    /* set up */

    struct event_base *event_base = event_init();

    setup_signal_handlers(&instance);

    instance_setup_server_socket(&instance);

    /* main loop */

#ifdef HAVE_LIBSYSTEMD
    /* tell systemd we're ready */
    sd_notify(0, "READY=1");
#endif

    event_dispatch();

    /* cleanup */

    event_base_free(event_base);

    return EXIT_SUCCESS;
} catch (const std::exception &e) {
    fprintf(stderr, "%s\n", e.what());
    return EXIT_FAILURE;
}
