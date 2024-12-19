// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Instance.hxx"
#include "Connection.hxx"
#include "Config.hxx"
#include "version.h"
#include "Log.hxx"
#include "util/PrintException.hxx"
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

static void
config_get(Config *config, int argc, char **argv)
{
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
setup_signal_handlers()
{
#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN);
#endif
}

int
main(int argc, char **argv)
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

	Log(1, "uoproxy v" VERSION
	       ", https://github.com/MaxKellermann/uoproxy\n");

	/* set up */

	setup_signal_handlers();

	instance_setup_server_socket(&instance);

	/* main loop */

	#ifdef HAVE_LIBSYSTEMD
	/* tell systemd we're ready */
	sd_notify(0, "READY=1");
#endif

	instance.event_loop.Run();

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
