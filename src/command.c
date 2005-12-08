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
#include <string.h>
#include <stdio.h>

#include "connection.h"
#include "handler.h"
#include "server.h"
#include "client.h"

packet_action_t handle_command(struct connection *c,
                               const char *command) {
    if (!c->in_game || c->current_server == NULL ||
        c->current_server->server == NULL)
        return PA_DROP;

    if (*command == 0) {
        uo_server_speak_console(c->current_server->server,
                                "uoproxy commands: % %reconnect");
    } else if (strcmp(command, "reconnect") == 0) {
        if (c->client == NULL) {
            uo_server_speak_console(c->current_server->server,
                                    "uoproxy: not connected");
        } else {
            uo_server_speak_console(c->current_server->server,
                                    "uoproxy: reconnecting");
            uo_client_dispose(c->client);
            c->client = NULL;
            c->reconnecting = 1;
        }
    } else if (strcmp(command, "char") == 0) {
        if (c->client == NULL) {
            uo_server_speak_console(c->current_server->server,
                                    "uoproxy: not connected");
        } else if (c->num_characters > 0) {
            char msg[256] = "uoproxy:";
            unsigned i, num;

            for (i = 0, num = 0; i < c->num_characters; i++) {
                if (c->characters[i].name[0]) {
                    sprintf(msg + strlen(msg), " %u=", i);
                    strncat(msg, c->characters[i].name,
                            sizeof(c->characters[i].name));
                    ++num;
                }
            }

            if (num > 0) {
                uo_server_speak_console(c->current_server->server, msg);
                return PA_DROP;
            }
        }

        uo_server_speak_console(c->current_server->server,
                                "uoproxy: no characters in list");
    } else {
        uo_server_speak_console(c->current_server->server,
                                "unknown uoproxy command, type '%' for help");
    }

    return PA_DROP;
}
