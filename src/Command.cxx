/*
 * uoproxy
 *
 * Copyright 2005-2020 Max Kellermann <max.kellermann@gmail.com>
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

#include "LinkedServer.hxx"
#include "Connection.hxx"
#include "Server.hxx"
#include "Client.hxx"
#include "Log.hxx"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static void
change_character(Connection *c,
                 LinkedServer *server,
                 unsigned idx)
{
    if (!c->client.char_list ||
        !c->client.char_list->IsValidCharacterIndex(idx)) {
        uo_server_speak_console(server->server,
                                "uoproxy: no character in slot");
        return;
    }

    c->character_index = idx;
    uo_server_speak_console(server->server,
                            "uoproxy: changing character");

    c->Reconnect();
}

void
connection_handle_command(LinkedServer *server, const char *command)
{
    Connection *c = server->connection;

    if (!c->IsInGame() || server->server == nullptr)
        return;

    if (*command == 0) {
        uo_server_speak_console(server->server,
                                "uoproxy commands: % %reconnect %char %drop %verbose");
    } else if (strcmp(command, "reconnect") == 0) {
        if (c->client.client == nullptr || c->client.reconnecting) {
            uo_server_speak_console(server->server,
                                    "uoproxy: not connected");
        } else {
            uo_server_speak_console(server->server,
                                    "uoproxy: reconnecting");
            c->Reconnect();
        }
    } else if (strcmp(command, "char") == 0) {
        char msg[1024] = "uoproxy:";

        if (!c->client.char_list) {
            uo_server_speak_console(server->server,
                                    "uoproxy: no characters in list");
            return;
        }

        const auto &char_list = *c->client.char_list;
        const unsigned n_chars = char_list.character_count;
        for (unsigned i = 0; i < n_chars; i++) {
            if (char_list.IsValidCharacterIndex(i)) {
                snprintf(msg + strlen(msg), sizeof(msg) - strlen(msg),
                         " %u=%s", i, char_list.character_info[i].name);
            }
        }

        uo_server_speak_console(server->server, msg);
    } else if (strncmp(command, "char ", 5) == 0) {
        if (command[5] >= '0' && command[5] <= '9' && command[6] == 0) {
            change_character(c, server, command[5] - '0');
        } else {
            uo_server_speak_console(server->server,
                                    "uoproxy: invalid %char syntax");
        }
    } else if (strcmp(command, "drop") == 0) {
        if (c->client.client == nullptr || c->client.reconnecting) {
            uo_server_speak_console(server->server,
                                    "uoproxy: not connected");
        } else if (c->client_version.protocol < PROTOCOL_6) {
            struct uo_packet_drop p = {
                .cmd = PCK_Drop,
                .serial = 0,
                .x = c->client.world.packet_start.x,
                .y = c->client.world.packet_start.y,
                .z = (int8_t)c->client.world.packet_start.z,
                .dest_serial = 0,
            };

            uo_client_send(c->client.client, &p, sizeof(p));
        } else {
            struct uo_packet_drop_6 p = {
                .cmd = PCK_Drop,
                .serial = 0,
                .x = c->client.world.packet_start.x,
                .y = c->client.world.packet_start.y,
                .z = (int8_t)c->client.world.packet_start.x,
                .unknown0 = 0,
                .dest_serial = 0,
            };

            uo_client_send(c->client.client, &p, sizeof(p));
        }
#ifndef DISABLE_LOGGING
    } else if (strncmp(command, "verbose", 7) == 0) {
        if (command[7] == ' ') {
            char *endptr;
            long new_verbose = strtol(command + 8, &endptr, 10);

            if (*endptr == 0) {
                verbose = (int)new_verbose;
                LogFormat(1, "verbose modified, new value=%d\n", verbose);
                return;
            }
        }

        uo_server_speak_console(server->server,
                                "uoproxy: invalid %verbose syntax");
#endif
    } else {
        uo_server_speak_console(server->server,
                                "unknown uoproxy command, type '%' for help");
    }
}
