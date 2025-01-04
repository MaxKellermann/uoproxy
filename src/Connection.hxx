// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "uo/WalkState.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "util/IntrusiveList.hxx"
#include "PacketStructs.hxx"
#include "World.hxx"
#include "Client.hxx"
#include "StatefulClient.hxx"

#include <array>

class SocketAddress;
class UniqueSocketDescriptor;
struct Instance;
struct Connection;
struct LinkedServer;

namespace UO {
class Client;
class Server;
}

struct Connection final : IntrusiveListHook<>, UO::ClientHandler {
	Instance &instance;

	/* flags */
	const bool background;

	/* reconnect */
	const bool autoreconnect;

	/* client stuff (= connection to server) */

	StatefulClient client;

	/**
	 * A timer which is used to schedule a reconnect.
	 */
	CoarseTimerEvent reconnect_timer;

	/* state */
	UO::CredentialsFragment credentials{};

	unsigned server_index = 0;

	unsigned character_index = 0;

	WalkState walk;

	/* sub-objects */

	IntrusiveList<LinkedServer> servers;

	Connection(Instance &_instance,
		   bool _background, bool _autoreconnect) noexcept;

	~Connection() noexcept;

	Connection(const Connection &) = delete;
	Connection &operator=(const Connection &) = delete;

	void Destroy() noexcept {
		unlink();
		delete this;
	}

	auto &GetEventLoop() const noexcept {
		return reconnect_timer.GetEventLoop();
	}

	bool IsInGame() const noexcept {
		return client.IsInGame();
	}

	bool CanAttach() const noexcept {
		return IsInGame() && client.char_list;
	}

	/**
	 * Throws on error.
	 */
	void Connect(SocketAddress server_address,
		     uint32_t seed, bool for_game_login);
	void Disconnect() noexcept;
	void Reconnect();
	void ScheduleReconnect() noexcept;

	void Add(LinkedServer &ls) noexcept;
	void Remove(LinkedServer &ls) noexcept;

	/**
	 * Remove the specified #LinkedServer and check if this is the
	 * last connection from a client; if so, it may destroy the whole
	 * #Connection.
	 */
	void RemoveCheckEmpty(LinkedServer &ls) noexcept;

	void BroadcastToInGameClients(std::span<const std::byte> src) noexcept;
	void BroadcastToInGameClientsExcept(std::span<const std::byte> src,
					    LinkedServer &except) noexcept;
	void BroadcastToInGameClientsDivert(ProtocolVersion new_protocol,
					    std::span<const std::byte> old_packet,
					    std::span<const std::byte> new_packet) noexcept;

	LinkedServer *FindZombie(const struct uo_packet_game_login &game_login) noexcept;

	void ClearWorld() noexcept {
		DeleteItems();
		DeleteMobiles();
	}

	void DeleteItems() noexcept;
	void DeleteMobiles() noexcept;

private:
	void DoReconnect() noexcept;
	void ReconnectTimerCallback() noexcept;

	/* virtual methods from UO::ClientHandler */
	bool OnClientPacket(std::span<const std::byte> src) override;
	bool OnClientDisconnect() noexcept override;
};

[[nodiscard]]
Connection *
connection_new(Instance *instance,
	       UniqueSocketDescriptor &&socket);

void
connection_speak_console(Connection *c, const char *msg);

/* walk */

void
connection_walk_server_removed(WalkState &state,
			       LinkedServer &ls) noexcept;

void
connection_walk_request(LinkedServer &ls,
			const struct uo_packet_walk &p);

void
connection_walk_cancel(Connection &c,
		       const struct uo_packet_walk_cancel &p);

void
connection_walk_ack(Connection &c,
		    const struct uo_packet_walk_ack &p);

/* attach */

void
attach_send_world(LinkedServer *ls);

/* command */

void
connection_handle_command(LinkedServer &ls, const char *command);
