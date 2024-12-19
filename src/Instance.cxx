// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Instance.hxx"
#include "Listener.hxx"
#include "Config.hxx"

#include <sys/socket.h> // for SOCK_STREAM

Instance::Instance(Config &_config) noexcept
	:config(_config)
{
	shutdown_listener.Enable();
}

Instance::~Instance() noexcept = default;

void
instance_setup_server_socket(Instance *instance)
{
	instance->listeners.emplace_front(*instance,
					  instance->config.listener.Create(SOCK_STREAM));
}

Connection *
Instance::FindAttachConnection(const UO::CredentialsFragment &credentials) noexcept
{
	for (auto &i : listeners)
		if (auto *f = i.FindAttachConnection(credentials))
			return f;

	return nullptr;
}

Connection *
Instance::FindAttachConnection(Connection &c) noexcept
{
	for (auto &i : listeners)
		if (auto *f = i.FindAttachConnection(c))
			return f;

	return nullptr;
}

LinkedServer *
Instance::FindZombie(const struct uo_packet_game_login &game_login) noexcept
{
	for (auto &i : listeners)
		if (auto *f = i.FindZombie(game_login))
			return f;

	return nullptr;
}

void
Instance::OnShutdown() noexcept
{
	shutdown_listener.Disable();

	listeners.clear();
}
