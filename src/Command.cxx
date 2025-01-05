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
		ls.server->SpeakConsole("uoproxy: no character in slot");
		return;
	}

	c.character_index = idx;
	ls.server->SpeakConsole("uoproxy: changing character");

	c.Reconnect();
}

void
LinkedServer::OnCommand(const char *command)
{
	Connection &c = *connection;

	if (!c.IsInGame() || server == nullptr)
		return;

	if (*command == 0) {
		server->SpeakConsole("uoproxy commands: % %reconnect %char %drop %verbose");
	} else if (strcmp(command, "reconnect") == 0) {
		server->SpeakConsole("uoproxy: reconnecting");
		c.Reconnect();
	} else if (strcmp(command, "char") == 0) {
		char msg[1024] = "uoproxy:";

		if (!c.client.char_list) {
			server->SpeakConsole("uoproxy: no characters in list");
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

		server->SpeakConsole(msg);
	} else if (strncmp(command, "char ", 5) == 0) {
		if (command[5] >= '0' && command[5] <= '9' && command[6] == 0) {
			change_character(c, *this, command[5] - '0');
		} else {
			server->SpeakConsole("uoproxy: invalid %char syntax");
		}
	} else if (strcmp(command, "drop") == 0) {
		if (c.client.client == nullptr || c.client.reconnecting) {
			server->SpeakConsole("uoproxy: not connected");
		} else if (c.client.version.protocol < ProtocolVersion::V6) {
			struct uo_packet_drop p = {
				.cmd = UO::Command::Drop,
				.serial = 0,
				.x = c.client.world.packet_start.x,
				.y = c.client.world.packet_start.y,
				.z = (int8_t)c.client.world.packet_start.z,
				.dest_serial = 0,
			};

			c.client.client->SendT(p);
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

			c.client.client->SendT(p);
		}
#ifndef DISABLE_LOGGING
	} else if (strncmp(command, "verbose", 7) == 0) {
		if (command[7] == ' ') {
			char *endptr;
			long new_verbose = strtol(command + 8, &endptr, 10);

			if (*endptr == 0) {
				verbose = (int)new_verbose;
				LogF(1, "verbose modified, new value={}", verbose);
				return;
			}
		}

		server->SpeakConsole("uoproxy: invalid %verbose syntax");
#endif
	} else {
		server->SpeakConsole("unknown uoproxy command, type '%' for help");
	}
}
