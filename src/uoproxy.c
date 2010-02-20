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

#include "instance.h"
#include "connection.h"
#include "config.h"
#include "version.h"
#include "compiler.h"
#include "log.h"

#ifndef WIN32
#include <sys/signal.h>
#include <signal.h>
#endif

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <winsock2.h>
#endif

#ifndef WIN32

static void
deinit_signals(struct instance *instance)
{
    (void)instance;
    event_del(&instance->sigterm_event);
    event_del(&instance->sigint_event);
    event_del(&instance->sigquit_event);
}

static void
delete_all_connections(struct list_head *head)
{
    struct connection *c, *n;

    list_for_each_entry_safe(c, n, head, siblings)
        connection_delete(c);
}

static void
exit_event_callback(int fd __attr_unused, short event __attr_unused, void *ctx)
{
    struct instance *instance = (struct instance*)ctx;

    if (instance->should_exit)
        return;

    instance->should_exit = true;

    deinit_signals(instance);

    if (instance->server_socket >= 0) {
        event_del(&instance->server_socket_event);
        close(instance->server_socket);
        instance->server_socket = -1;
    }

    delete_all_connections(&instance->connections);
}

#endif

static void config_get(struct config *config, int argc, char **argv) {
    const char *home;
    char path[4096];
    int ret;

    memset(config, 0, sizeof(*config));
    config->autoreconnect = true;

    home = getenv("HOME");
    if (home == NULL) {
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
setup_signal_handlers(struct instance *instance)
{
#ifdef WIN32
    (void)instance;
#else
    signal(SIGPIPE, SIG_IGN);

    event_set(&instance->sigterm_event, SIGTERM, EV_SIGNAL|EV_PERSIST,
              exit_event_callback, instance);
    event_add(&instance->sigterm_event, NULL);

    event_set(&instance->sigint_event, SIGINT, EV_SIGNAL|EV_PERSIST,
              exit_event_callback, instance);
    event_add(&instance->sigint_event, NULL);

    event_set(&instance->sigquit_event, SIGQUIT, EV_SIGNAL|EV_PERSIST,
              exit_event_callback, instance);
    event_add(&instance->sigquit_event, NULL);
#endif
}

int main(int argc, char **argv) {
    struct config config;
    struct instance instance = {
        .config = &config,
    };

    INIT_LIST_HEAD(&instance.connections);

    /* WinSock */

#ifdef WIN32
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

    log(1, "uoproxy v" VERSION
        ", http://max.kellermann.name/projects/uoproxy/\n");

    /* set up */

    struct event_base *event_base = event_init();

    setup_signal_handlers(&instance);

    instance_setup_server_socket(&instance);

    instance_daemonize(&instance);

    /* main loop */

    event_dispatch();

    /* cleanup */

    event_base_free(event_base);

    config_dispose(&config);

    return 0;
}
