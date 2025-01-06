// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "uo/Packets.hxx"
#include "uo/WalkState.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "util/IntrusiveList.hxx"
#include "World.hxx"
#include "PacketHandler.hxx"
#include "StatefulClient.hxx"

#include <array>

class SocketAddress;
class UniqueSocketDescriptor;
struct Instance;
struct Connection;
struct LinkedServer;

namespace UO {
enum class Command : uint8_t;
class Client;
class Server;
}

enum class PacketAction;

struct Connection final : IntrusiveListHook<>, UO::PacketHandler {
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

	/** broadcast a message to all clients */
	void SpeakConsole(const char *msg);

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

	void Welcome();

	void Resynchronize();
	void OnWalkCancel(const struct uo_packet_walk_cancel &p);
	void OnWalkAck(const struct uo_packet_walk_ack &p);

	PacketAction HandleMobileStatus(std::span<const std::byte> src);
	PacketAction HandleWorldItem(std::span<const std::byte> src);
	PacketAction HandleStart(std::span<const std::byte> src);
	PacketAction HandleSpeakAscii(std::span<const std::byte> src);
	PacketAction HandleDelete(std::span<const std::byte> src);
	PacketAction HandleMobileUpdate(std::span<const std::byte> src);
	PacketAction HandleWalkCancel(std::span<const std::byte> src);
	PacketAction HandleWalkAck(std::span<const std::byte> src);
	PacketAction HandleContainerOpen(std::span<const std::byte> src);
	PacketAction HandleContainerUpdate(std::span<const std::byte> src);
	PacketAction HandleEquip(std::span<const std::byte> src);
	PacketAction HandleContainerContent(std::span<const std::byte> src);
	PacketAction HandlePersonalLightLevel(std::span<const std::byte> src);
	PacketAction HandleGlobalLightLevel(std::span<const std::byte> src);
	PacketAction HandlePopupMessage(std::span<const std::byte> src);
	PacketAction HandleLoginComplete(std::span<const std::byte> src);
	PacketAction HandleTarget(std::span<const std::byte> src);
	PacketAction HandleWarMode(std::span<const std::byte> src);
	PacketAction HandlePing(std::span<const std::byte> src);
	PacketAction HandleZoneChange(std::span<const std::byte> src);
	PacketAction HandleMobileMoving(std::span<const std::byte> src);
	PacketAction HandleMobileIncoming(std::span<const std::byte> src);
	PacketAction HandleCharList(std::span<const std::byte> src);
	PacketAction HandleAccountLoginReject(std::span<const std::byte> src);
	PacketAction HandleRelay(std::span<const std::byte> src);
	PacketAction HandleServerList(std::span<const std::byte> src);
	PacketAction HandleSpeakUnicode(std::span<const std::byte> src);
	PacketAction HandleSupportedFeatures(std::span<const std::byte> src);
	PacketAction HandleSeason(std::span<const std::byte> src);
	PacketAction HandleClientVersion(std::span<const std::byte> src);
	PacketAction HandleExtended(std::span<const std::byte> src);
	PacketAction HandleWorldItem7(std::span<const std::byte> src);
	PacketAction HandleProtocolExtension(std::span<const std::byte> src);

	using PacketHandlerMethod = PacketAction (Connection::*)(std::span<const std::byte> src);
	struct CommandHandler {
		UO::Command cmd;
		PacketHandlerMethod method;
	};

	static const CommandHandler command_handlers[];

	/* virtual methods from UO::ClientHandler */
	OnPacketResult OnPacket(std::span<const std::byte> src) override;
	bool OnDisconnect() noexcept override;
};

[[nodiscard]]
Connection *
connection_new(Instance *instance,
	       UniqueSocketDescriptor &&socket);
