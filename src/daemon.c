/*
 * uoproxy
 *
 * (c) 2005-2010 Max Kellermann <max@duempel.org>
 *
 * based on code from "Ultimate Melange"
 * Copyright (C) 2000 Axel Kittenberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "instance.h"
#include "config.h"
#include "log.h"

#ifndef DISABLE_DAEMON_CODE

#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <grp.h>

void instance_daemonize(struct instance *instance) {
    struct config *config = instance->config;
    int ret, parentfd = -1, loggerfd = -1;
    pid_t logger_pid;

    /* daemonize */
    if (!config->no_daemon && getppid() != 1) {
        int fds[2];
        pid_t pid;

        ret = pipe(fds);
        if (ret < 0) {
            perror("pipe failed");
            exit(1);
        }

        pid = fork();
        if (pid < 0) {
            perror("fork failed");
            exit(1);
        }

        if (pid > 0) {
            int status;
            fd_set rfds;
            char buffer[256];
            struct timeval tv;
            pid_t pid2;

            close(fds[1]);

            log(4, "waiting for daemon process %ld\n", (long)pid);

            do {
                FD_ZERO(&rfds);
                FD_SET(fds[0], &rfds);
                tv.tv_sec = 0;
                tv.tv_usec = 100000;
                ret = select(fds[0] + 1, &rfds, NULL, NULL, &tv);
                if (ret > 0 && read(fds[0], buffer, sizeof(buffer)) > 0) {
                    log(2, "detaching %ld\n", (long)getpid());
                    exit(0);
                }

                pid2 = waitpid(pid, &status, WNOHANG);
            } while (pid2 <= 0);

            log(3, "daemon process exited with %d\n",
                WEXITSTATUS(status));
            exit(WEXITSTATUS(status));
        }

        close(fds[0]);
        parentfd = fds[1];

        setsid();

        close(0);

        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);

        log(3, "daemonized as pid %ld\n", (long)getpid());
    }

    /* write PID file */
    if (config->pidfile != NULL) {
        FILE *file;

        file = fopen(config->pidfile, "w");
        if (file == NULL) {
            fprintf(stderr, "failed to create '%s': %s\n",
                    config->pidfile, strerror(errno));
            exit(1);
        }

        fprintf(file, "%ld\n", (long)getpid());
        fclose(file);
    }

    /* start logger process */
    if (config->logger != NULL) {
        int fds[2];

        log(3, "starting logger '%s'\n", config->logger);

        ret = pipe(fds);
        if (ret < 0) {
            perror("pipe failed");
            exit(1);
        }

        logger_pid = fork();
        if (logger_pid < 0) {
            perror("fork failed");
            exit(1);
        } else if (logger_pid == 0) {
            if (fds[0] != 0) {
                dup2(fds[0], 0);
                close(fds[0]);
            }

            close(fds[1]);
            close(1);
            close(2);
            close(instance->server_socket);
#ifdef HAVE_DEV_RANDOM
            close(*randomfdp);
#endif

            execl("/bin/sh", "sh", "-c", config->logger, NULL);
            exit(1);
        }

        log(2, "logger started as pid %ld\n", (long)logger_pid);

        close(fds[0]);
        loggerfd = fds[1];

        log(3, "logger %ld connected\n", (long)logger_pid);
    }

    /* chroot */
    if (config->chroot_dir != NULL) {
        ret = chroot(config->chroot_dir);
        if (ret < 0) {
            fprintf(stderr, "chroot '%s' failed: %s\n",
                    config->chroot_dir, strerror(errno));
            exit(1);
        }
    }

    chdir("/");

    /* setuid */
    if (config->uid > 0) {
        ret = setgroups(0, NULL);
        if (ret < 0) {
            perror("setgroups failed");
            exit(1);
        }

        ret = setregid(config->gid, config->gid);
        if (ret < 0) {
            perror("setgid failed");
            exit(1);
        }

        ret = setreuid(config->uid, config->uid);
        if (ret < 0) {
            perror("setuid failed");
            exit(1);
        }
    } else if (getuid() == 0) {
        /* drop a real_uid root */
        setuid(geteuid());
    }

    /* send parent process a signal */
    if (parentfd >= 0) {
        log(4, "closing parent pipe %d\n", parentfd);
        write(parentfd, &parentfd, sizeof(parentfd));
        close(parentfd);
    }

    /* now connect logger */
    if (loggerfd >= 0) {
        dup2(loggerfd, 1);
        dup2(loggerfd, 2);
        close(loggerfd);
    }
}

#endif /* DISABLE_DAEMON_CODE */
