// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "LinkedServer.hxx"
#include "Connection.hxx"
#include "Server.hxx"
#include "Client.hxx"
#include "Log.hxx"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static void
change_character(Connection &c, LinkedServer &ls, unsigned idx)
{
    if (!c.client.char_list ||
        !c.client.char_list->IsValidCharacterIndex(idx)) {
        uo_server_speak_console(ls.server,
                                "uoproxy: no character in slot");
        return;
    }

    c.character_index = idx;
    uo_server_speak_console(ls.server,
                            "uoproxy: changing character");

    c.Reconnect();
}

void
connection_handle_command(LinkedServer &ls, const char *command)
{
    Connection &c = *ls.connection;

    if (!c.IsInGame() || ls.server == nullptr)
        return;

    if (*command == 0) {
        uo_server_speak_console(ls.server,
                                "uoproxy commands: % %reconnect %char %drop %verbose");
    } else if (strcmp(command, "reconnect") == 0) {
        uo_server_speak_console(ls.server,
                                "uoproxy: reconnecting");
        c.Reconnect();
    } else if (strcmp(command, "char") == 0) {
        char msg[1024] = "uoproxy:";

        if (!c.client.char_list) {
            uo_server_speak_console(ls.server,
                                    "uoproxy: no characters in list");
            return;
        }

        const auto &char_list = *c.client.char_list;
        const unsigned n_chars = char_list.character_count;
        for (unsigned i = 0; i < n_chars; i++) {
            if (char_list.IsValidCharacterIndex(i)) {
                snprintf(msg + strlen(msg), sizeof(msg) - strlen(msg),
                         " %u=%s", i, char_list.character_info[i].name);
            }
        }

        uo_server_speak_console(ls.server, msg);
    } else if (strncmp(command, "char ", 5) == 0) {
        if (command[5] >= '0' && command[5] <= '9' && command[6] == 0) {
            change_character(c, ls, command[5] - '0');
        } else {
            uo_server_speak_console(ls.server,
                                    "uoproxy: invalid %char syntax");
        }
    } else if (strcmp(command, "drop") == 0) {
        if (c.client.client == nullptr || c.client.reconnecting) {
            uo_server_speak_console(ls.server,
                                    "uoproxy: not connected");
        } else if (c.client.version.protocol < PROTOCOL_6) {
            struct uo_packet_drop p = {
                .cmd = UO::Command::Drop,
                .serial = 0,
                .x = c.client.world.packet_start.x,
                .y = c.client.world.packet_start.y,
                .z = (int8_t)c.client.world.packet_start.z,
                .dest_serial = 0,
            };

            uo_client_send(c.client.client, &p, sizeof(p));
        } else {
            struct uo_packet_drop_6 p = {
                .cmd = UO::Command::Drop,
                .serial = 0,
                .x = c.client.world.packet_start.x,
                .y = c.client.world.packet_start.y,
                .z = (int8_t)c.client.world.packet_start.x,
                .unknown0 = 0,
                .dest_serial = 0,
            };

            uo_client_send(c.client.client, &p, sizeof(p));
        }
#ifndef DISABLE_LOGGING
    } else if (strncmp(command, "verbose", 7) == 0) {
        if (command[7] == ' ') {
            char *endptr;
            long new_verbose = strtol(command + 8, &endptr, 10);

            if (*endptr == 0) {
                verbose = (int)new_verbose;
                ls.LogF(1, "verbose modified, new value={}", verbose);
                return;
            }
        }

        uo_server_speak_console(ls.server,
                                "uoproxy: invalid %verbose syntax");
#endif
    } else {
        uo_server_speak_console(ls.server,
                                "unknown uoproxy command, type '%' for help");
    }
}
