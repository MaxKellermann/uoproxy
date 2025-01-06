// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "PacketHandler.hxx"
#include "CVersion.hxx"
#include "event/CoarseTimerEvent.hxx"
#include "util/IntrusiveList.hxx"

#include <fmt/core.h>

#include <cstdint>
#include <memory>
#include <string_view>

struct Connection;

namespace UO {
enum class Command : uint8_t;
class Server;
}

enum class PacketAction;

/**
 * This object manages one connection from a UO client to uoproxy
 * (where uoproxy acts as a UO "server", therefore the class name).
 * It may be one of of several clients sharing the connection to the
 * real UO server.
 */
struct LinkedServer final : IntrusiveListHook<>, UO::PacketHandler {
	Connection *connection = nullptr;

	std::unique_ptr<UO::Server> server;

	ClientVersion client_version;

	CoarseTimerEvent zombie_timeout; /**< zombies time out and auto-reap themselves
					    after 5 seconds using this timer */

	/**
	 * Identifier for this object in log messages.
	 */
	const unsigned id;

	static unsigned id_counter;

	uint32_t auth_id; /**< unique identifier for this linked_server used in
			     redirect handling to locate the zombied
			     linked_server */

	bool welcome = false;

	enum class State : uint8_t {
		/**
		 * The initial state, nothing has been received yet.  We're
		 * waiting for AccountLogin or GameLogin.
		 */
		INIT,

		/**
		 * We have received AccountLogin from this client, and now
		 * we're waiting for a server list from the real login server.
		 * As soon as we receive it, we forward it to this client, and
		 * the state changes to #SERVER_LIST.
		 */
		ACCOUNT_LOGIN,

		/**
		 * We have sent the server list to this client, and we're
		 * waiting for PlayServer from this client.
		 */
		SERVER_LIST,

		/**
		 * We have received PlayServer from this client, and now we're
		 * waiting for a character list from the real game server.  As
		 * soon as we receive it, we forward it to this client, and
		 * the state changes to #CHAR_LIST.
		 */
		PLAY_SERVER,

		/**
		 * We have sent RelayServer to this client, and we expecting it to
		 * close the connection and create a new one to us.  When the
		 * connection is closed, this object remains for a few seconds, as
		 * a placeholder for the new connection.
		 */
		RELAY_SERVER,

		/**
		 * We have received GameLogin from this client, and now we're
		 * waiting for a character list from the real game server.  As
		 * soon as we receive it, we forward it to this client, and
		 * the state changes to #CHAR_LIST.
		 */
		GAME_LOGIN,

		/**
		 * We have sent the character list to this client, and we're
		 * waiting for PlayCharacter from this client.
		 */
		CHAR_LIST,

		/**
		 * We have received PlayCharacter from this client, and we're
		 * waiting for the real game server to send world data to us.
		 */
		PLAY_CHAR,

		/**
		 * This client has received world data, and is playing.
		 */
		IN_GAME,
	} state = State::INIT;

	[[nodiscard]]
	LinkedServer(EventLoop &event_loop, UniqueSocketDescriptor &&s);
	~LinkedServer() noexcept;

	LinkedServer(const LinkedServer &) = delete;
	LinkedServer &operator=(const LinkedServer &) = delete;

	bool IsZombie() const noexcept {
		return state == State::RELAY_SERVER && server == nullptr;
	}

	bool IsAtAccountLogin() const noexcept {
		return state == State::ACCOUNT_LOGIN;
	}

	/**
	 * Can we forward in-game packets to the client connected to this
	 * object?
	 */
	bool IsInGame() const noexcept {
		return state == State::IN_GAME;
	}

	void LogVFmt(unsigned level, fmt::string_view format_str, fmt::format_args args) noexcept;

	template<typename S, typename... Args>
	void LogF(unsigned level, const S &format_str, Args&&... args) noexcept {
		return LogVFmt(level, format_str, fmt::make_format_args(args...));
	}

	void Send(std::span<const std::byte> src);

	void SendDivert(ProtocolVersion new_protocol,
			std::span<const std::byte> old_packet,
			std::span<const std::byte> new_packet) noexcept;

	void SpeakConsole(std::string_view text);

private:
	void ZombieTimeoutCallback() noexcept;

	void RedirectToSelf();

	void OnWalkRequest(const struct uo_packet_walk &p);

	void OnCommand(std::string_view command);
	PacketAction HandleTalk(std::string_view text);

	PacketAction HandleCreateCharacter(std::span<const std::byte> src);
	PacketAction HandleWalk(std::span<const std::byte> src);
	PacketAction HandleTalkAscii(std::span<const std::byte> src);
	PacketAction HandleUse(std::span<const std::byte> src);
	PacketAction HandleAction(std::span<const std::byte> src);
	PacketAction HandleLiftRequest(std::span<const std::byte> src);
	PacketAction HandleDrop(std::span<const std::byte> src);
	PacketAction HandleResynchronize(std::span<const std::byte> src);
	PacketAction HandleTarget(std::span<const std::byte> src);
	PacketAction HandlePing(std::span<const std::byte> src);
	PacketAction HandleAccountLogin(std::span<const std::byte> src);
	PacketAction HandleGameLogin(std::span<const std::byte> src);
	PacketAction HandlePlayCharacter(std::span<const std::byte> src);
	PacketAction HandlePlayServer(std::span<const std::byte> src);
	PacketAction HandleSpy(std::span<const std::byte> src);
	PacketAction HandleTalkUnicode(std::span<const std::byte> src);
	PacketAction HandleGumpResponse(std::span<const std::byte> src);
	PacketAction HandleClientVersion(std::span<const std::byte> src);
	PacketAction HandleExtended(std::span<const std::byte> src);
	PacketAction HandleHardware(std::span<const std::byte> src);
	PacketAction HandleSeed(std::span<const std::byte> src);

	using PacketHandlerMethod = PacketAction (LinkedServer::*)(std::span<const std::byte> src);
	struct CommandHandler {
		UO::Command cmd;
		PacketHandlerMethod method;
	};

	static const CommandHandler command_handlers[];

	/* virtual methods from UO::ServerHandler */
	OnPacketResult OnPacket(std::span<const std::byte> src) override;
	bool OnDisconnect() noexcept override;
};
