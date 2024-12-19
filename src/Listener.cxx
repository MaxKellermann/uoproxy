// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Listener.hxx"
#include "Instance.hxx"
#include "Connection.hxx"
#include "net/SocketAddress.hxx"
#include "util/PrintException.hxx"

Listener::Listener(Instance &_instance, UniqueSocketDescriptor &&socket)
	:ServerSocket(_instance.event_loop, std::move(socket)),
	 instance(_instance) {}

Listener::~Listener() noexcept
{
	connections.clear_and_dispose([](Connection *c) {
		delete c;
	});
}

void
Listener::OnAccept(UniqueSocketDescriptor fd,
		   [[maybe_unused]] SocketAddress address) noexcept
{
	auto *c = connection_new(&instance, std::move(fd));
	connections.push_front(*c);
}

void
Listener::OnAcceptError(std::exception_ptr error) noexcept
{
	PrintException(std::move(error));
}

Connection *
Listener::FindAttachConnection(const UO::CredentialsFragment &credentials) noexcept
{
	for (auto &i : connections)
		if (i.CanAttach() && credentials == i.credentials)
			return &i;

	return nullptr;
}

Connection *
Listener::FindAttachConnection(Connection &c) noexcept
{
	for (auto &i : connections)
		if (&i != &c && i.CanAttach() &&
		    c.credentials == i.credentials &&
		    c.server_index == i.server_index)
			return &i;

	return nullptr;
}

LinkedServer *
Listener::FindZombie(const struct uo_packet_game_login &game_login) noexcept
{
	for (auto &i : connections) {
		auto *zombie = i.FindZombie(game_login);
		if (zombie != nullptr)
			return zombie;
	}

	return nullptr;
}
