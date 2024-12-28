// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "net/AllocatedSocketAddress.hxx"
#include "net/AddressInfo.hxx"
#include "net/SocketConfig.hxx"

#include <string>
#include <vector>

struct GameServerConfig {
	const std::string name;
	AllocatedSocketAddress address;

	[[nodiscard]]
	GameServerConfig(const char *_name, SocketAddress _address) noexcept
		:name(_name), address(_address) {}
};

struct Config {
	SocketConfig listener;

	/**
	 * The address of the SOCKS4 proxy server.
	 */
	AddressInfoList socks4_address;

	AddressInfoList login_address;

	std::vector<GameServerConfig> game_servers;
	bool background = false, autoreconnect = true, antispy = false, razor_workaround = false;

	/**
	 * Always full light level?
	 */
	bool light = false;

	std::string client_version;

	Config() noexcept;
	~Config() noexcept;
};

/** read configuration options from the command line */
void parse_cmdline(Config *config, int argc, char **argv);

int config_read_file(Config *config, const char *path);
