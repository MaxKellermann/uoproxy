// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "LinkedServer.hxx"
#include "Connection.hxx"
#include "Server.hxx"
#include "Client.hxx"
#include "Log.hxx"
#include "util/NumberParser.hxx"
#include "util/StringCompare.hxx"

#include <stdio.h>

using std::string_view_literals::operator""sv;

static void
change_character(Connection &c, LinkedServer &ls, unsigned idx)
{
	if (!c.client.char_list ||
	    !c.client.char_list->IsValidCharacterIndex(idx)) {
		ls.SpeakConsole("uoproxy: no character in slot");
		return;
	}

	c.character_index = idx;
	ls.SpeakConsole("uoproxy: changing character");

	c.Reconnect();
}

void
LinkedServer::OnCommand(std::string_view command)
{
	Connection &c = *connection;

	if (!c.IsInGame() || server == nullptr)
		return;

	if (command.empty()) {
		SpeakConsole("uoproxy commands: % %reconnect %char %drop %verbose");
	} else if (command == "reconnect"sv) {
		SpeakConsole("uoproxy: reconnecting");
		c.Reconnect();
	} else if (command == "char"sv) {
		char msg[1024] = "uoproxy:";

		if (!c.client.char_list) {
			SpeakConsole("uoproxy: no characters in list");
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

		SpeakConsole(msg);
	} else if (SkipPrefix(command, "char "sv)) {
		if (command.size() == 1 && command.front() >= '0' && command.front() <= '9') {
			change_character(c, *this, command.front() - '0');
		} else {
			SpeakConsole("uoproxy: invalid %char syntax");
		}
	} else if (command == "drop"sv) {
		if (c.client.client == nullptr || c.client.reconnecting) {
			SpeakConsole("uoproxy: not connected");
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
	} else if (SkipPrefix(command, "verbose"sv)) {
		if (command.size() >= 2 && command.front() == ' ') {
			const auto new_verbose = ParseInteger<unsigned>(command.substr(1));
			if (new_verbose) {
				verbose = *new_verbose;
				LogF(1, "verbose modified, new value={}", verbose);
				return;
			}
		}

		SpeakConsole("uoproxy: invalid %verbose syntax");
#endif
	} else {
		SpeakConsole("unknown uoproxy command, type '%' for help");
	}
}
