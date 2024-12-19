// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Connection.hxx"
#include "LinkedServer.hxx"
#include "Instance.hxx"
#include "Config.hxx"
#include "Log.hxx"

#include <assert.h>
#include <stdlib.h>
#include <errno.h>

Connection::Connection(Instance &_instance,
		       bool _background, bool _autoreconnect) noexcept
	:instance(_instance), background(_background),
	 autoreconnect(_autoreconnect),
	 client(instance.event_loop),
	 reconnect_timer(instance.event_loop, BIND_THIS_METHOD(ReconnectTimerCallback))
{
}

Connection *
connection_new(Instance *instance,
	       UniqueSocketDescriptor &&socket)
{
	auto *ls = new LinkedServer(instance->event_loop, std::move(socket));

	auto *c = new Connection(*instance,
				 instance->config.background,
				 instance->config.autoreconnect);

	if (!instance->config.client_version.empty()) {
		c->client.version.Set(instance->config.client_version);
		ls->LogF(2, "configured client version {:?}, protocol {:?}",
			 c->client.version.packet->version,
			 protocol_name(c->client.version.protocol));
	}

	c->Add(*ls);

	return c;
}

Connection::~Connection() noexcept
{
	servers.clear_and_dispose([](LinkedServer *ls){ delete ls; });

	Disconnect();
}
