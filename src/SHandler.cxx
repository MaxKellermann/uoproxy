/*
 * uoproxy
 *
 * Copyright 2005-2020 Max Kellermann <max.kellermann@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "Handler.hxx"
#include "Instance.hxx"
#include "PacketStructs.hxx"
#include "VerifyPacket.hxx"
#include "Connection.hxx"
#include "LinkedServer.hxx"
#include "Server.hxx"
#include "Client.hxx"
#include "Config.hxx"
#include "Log.hxx"
#include "version.h"
#include "Bridge.hxx"

#include <assert.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

static void welcome(Connection &c) {
    for (auto &ls : c.servers) {
        if (ls.IsInGame() && !ls.welcome) {
            uo_server_speak_console(ls.server, "Welcome to uoproxy v" VERSION "!  "
                                    "https://github.com/MaxKellermann/uoproxy");
            ls.welcome = true;
        }
    }
}

/** send a HardwareInfo packet to the server, containing inane
    information, to overwrite its old database entry */
static void send_antispy(UO::Client *client) {
    static constexpr struct uo_packet_hardware p{
        .cmd = PCK_Hardware,
        .unknown0 = 2,
        .instance_id = 0xdeadbeef,
        .os_major = 5,
        .os_minor = 0,
        .os_revision = 0,
        .cpu_manufacturer = 3,
        .cpu_family = 6,
        .cpu_model = 8,
        .cpu_clock = 997,
        .cpu_quantity = 8,
        .physical_memory = 256,
        .screen_width = 1600,
        .screen_height = 1200,
        .screen_depth = 32,
        .dx_major = 9,
        .dx_minor = 0,
        .vc_description = {
            'S', 0, '3', 0, ' ', 0, 'T', 0, 'r', 0, 'i', 0, 'o',
        },
        .vc_vendor_id = 0,
        .vc_device_id = 0,
        .vc_memory = 4,
        .distribution = 2,
        .clients_running = 1,
        .clients_installed = 1,
        .partial_installed = 0,
        .language = { 'e', 0, 'n', 0, 'u', 0 },
        .unknown1 = {},
    };

    uo_client_send(client, &p, sizeof(p));
}

static PacketAction
handle_mobile_status(Connection &c, const void *data, size_t length)
{
    auto p = (const struct uo_packet_mobile_status *)data;

    (void)length;

    c.client.world.Apply(*p);
    return PacketAction::ACCEPT;
}

static PacketAction
handle_world_item(Connection &c, const void *data, [[maybe_unused]] size_t length)
{
    auto p = (const struct uo_packet_world_item *)data;

    assert(length <= sizeof(*p));

    c.client.world.Apply(*p);
    return PacketAction::ACCEPT;
}

static PacketAction
handle_start(Connection &c, const void *data, [[maybe_unused]] size_t length)
{
    auto p = (const struct uo_packet_start *)data;

    assert(length == sizeof(*p));

    c.client.world.packet_start = *p;

    /* if we're auto-reconnecting, this is the point where it
       succeeded */
    c.client.reconnecting = false;

    c.walk.seq_next = 0;

    for (auto &ls : c.servers)
        ls.state = LinkedServer::State::IN_GAME;

    return PacketAction::ACCEPT;
}

static PacketAction
handle_speak_ascii(Connection &c, const void *data, size_t length)
{
    (void)data;
    (void)length;

    welcome(c);

    return PacketAction::ACCEPT;
}

static PacketAction
handle_delete(Connection &c, const void *data, [[maybe_unused]] size_t length)
{
    auto p = (const struct uo_packet_delete *)data;

    assert(length == sizeof(*p));

    c.client.world.RemoveSerial(p->serial);
    return PacketAction::ACCEPT;
}

static PacketAction
handle_mobile_update(Connection &c, const void *data, [[maybe_unused]] size_t length)
{
    auto p = (const struct uo_packet_mobile_update *)data;

    assert(length == sizeof(*p));

    c.client.world.Apply(*p);
    return PacketAction::ACCEPT;
}

static PacketAction
handle_walk_cancel(Connection &c, const void *data, [[maybe_unused]] size_t length)
{
    auto p = (const struct uo_packet_walk_cancel *)data;

    assert(length == sizeof(*p));

    if (!c.IsInGame())
        return PacketAction::DISCONNECT;

    connection_walk_cancel(&c, p);

    return PacketAction::DROP;
}

static PacketAction
handle_walk_ack(Connection &c, const void *data, [[maybe_unused]] size_t length)
{
    auto p = (const struct uo_packet_walk_ack *)data;

    assert(length == sizeof(*p));

    connection_walk_ack(&c, p);

    /* XXX: x/y/z etc. */

    return PacketAction::DROP;
}

static PacketAction
handle_container_open(Connection &c, const void *data, size_t length)
{
    if (c.client_version.IsDefined() &&
        c.client_version.protocol >= PROTOCOL_7) {
        auto p = (const struct uo_packet_container_open_7 *)data;

        c.client.world.Apply(*p);

        c.BroadcastToInGameClientsDivert(PROTOCOL_7,
                                          &p->base, sizeof(p->base),
                                          data, length);
        return PacketAction::DROP;
    } else {
        auto p = (const struct uo_packet_container_open *)data;
        assert(length == sizeof(*p));

        c.client.world.Apply(*p);

        const struct uo_packet_container_open_7 p7 = {
            .base = *p,
            .zero = 0x00,
            .x7d = 0x7d,
        };

        c.BroadcastToInGameClientsDivert(PROTOCOL_7,
                                          p, sizeof(*p),
                                          &p7, sizeof(p7));

        return PacketAction::DROP;
    }
}

static PacketAction
handle_container_update(Connection &c, const void *data, size_t length)
{
    if (c.client_version.protocol < PROTOCOL_6) {
        auto p = (const struct uo_packet_container_update *)data;
        struct uo_packet_container_update_6 p6;

        assert(length == sizeof(*p));

        container_update_5_to_6(&p6, p);

        c.client.world.Apply(p6);

        c.BroadcastToInGameClientsDivert(PROTOCOL_6,
                                          data, length,
                                          &p6, sizeof(p6));
    } else {
        auto p = (const struct uo_packet_container_update_6 *)data;
        struct uo_packet_container_update p5;

        assert(length == sizeof(*p));

        container_update_6_to_5(&p5, p);

        c.client.world.Apply(*p);

        c.BroadcastToInGameClientsDivert(PROTOCOL_6,
                                          &p5, sizeof(p5),
                                          data, length);
    }

    return PacketAction::DROP;
}

static PacketAction
handle_equip(Connection &c, const void *data, [[maybe_unused]] size_t length)
{
    auto p = (const struct uo_packet_equip *)data;

    assert(length == sizeof(*p));

    c.client.world.Apply(*p);

    return PacketAction::ACCEPT;
}

static PacketAction
handle_container_content(Connection &c, const void *data, size_t length)
{
    if (packet_verify_container_content((const uo_packet_container_content *)data, length)) {
        /* protocol v5 */
        auto p = (const struct uo_packet_container_content *)data;

        const auto p6 = container_content_5_to_6(p);

        c.client.world.Apply(*p6);

        c.BroadcastToInGameClientsDivert(PROTOCOL_6,
                                          data, length,
                                          p6.get(), p6.size());
    } else if (packet_verify_container_content_6((const uo_packet_container_content_6 *)data, length)) {
        /* protocol v6 */
        auto p = (const struct uo_packet_container_content_6 *)data;

        c.client.world.Apply(*p);

        const auto p5 = container_content_6_to_5(p);
        c.BroadcastToInGameClientsDivert(PROTOCOL_6,
                                          p5.get(), p5.size(),
                                          data, length);
    } else
        return PacketAction::DISCONNECT;

    return PacketAction::DROP;
}

static PacketAction
handle_personal_light_level(Connection &c, const void *data, [[maybe_unused]] size_t length)
{
    auto p = (const struct uo_packet_personal_light_level *)data;

    assert(length == sizeof(*p));

    if (c.instance.config.light)
        return PacketAction::DROP;

    if (c.client.world.packet_start.serial == p->serial)
        c.client.world.packet_personal_light_level = *p;

    return PacketAction::ACCEPT;
}

static PacketAction
handle_global_light_level(Connection &c, const void *data, [[maybe_unused]] size_t length)
{
    auto p = (const struct uo_packet_global_light_level *)data;

    assert(length == sizeof(*p));

    if (c.instance.config.light)
        return PacketAction::DROP;

    c.client.world.packet_global_light_level = *p;

    return PacketAction::ACCEPT;
}

static PacketAction
handle_popup_message(Connection &c, const void *data, [[maybe_unused]] size_t length)
{
    auto p = (const struct uo_packet_popup_message *)data;

    assert(length == sizeof(*p));

    if (c.client.reconnecting) {
        if (p->msg == 0x05) {
            connection_speak_console(&c, "previous character is still online, trying again");
        } else {
            connection_speak_console(&c, "character change failed, trying again");
        }

        c.ScheduleReconnect();
        return PacketAction::DELETED;
    }

    return PacketAction::ACCEPT;
}

static PacketAction
handle_login_complete(Connection &c, const void *data, size_t length)
{
    (void)data;
    (void)length;

    if (c.instance.config.antispy)
        send_antispy(c.client.client);

    return PacketAction::ACCEPT;
}

static PacketAction
handle_target(Connection &c, const void *data, [[maybe_unused]] size_t length)
{
    auto p = (const struct uo_packet_target *)data;

    assert(length == sizeof(*p));

    c.client.world.packet_target = *p;

    return PacketAction::ACCEPT;
}

static PacketAction
handle_war_mode(Connection &c, const void *data, [[maybe_unused]] size_t length)
{
    auto p = (const struct uo_packet_war_mode *)data;

    assert(length == sizeof(*p));

    c.client.world.packet_war_mode = *p;

    return PacketAction::ACCEPT;
}

static PacketAction
handle_ping(Connection &c, const void *data, [[maybe_unused]] size_t length)
{
    auto p = (const struct uo_packet_ping *)data;

    assert(length == sizeof(*p));

    c.client.ping_ack = p->id;

    return PacketAction::DROP;
}

static PacketAction
handle_zone_change(Connection &c, const void *data, [[maybe_unused]] size_t length)
{
    auto p = (const struct uo_packet_zone_change *)data;

    assert(length == sizeof(*p));

    c.client.world.Apply(*p);
    return PacketAction::ACCEPT;
}

static PacketAction
handle_mobile_moving(Connection &c, const void *data, [[maybe_unused]] size_t length)
{
    auto p = (const struct uo_packet_mobile_moving *)data;

    assert(length == sizeof(*p));

    c.client.world.Apply(*p);
    return PacketAction::ACCEPT;
}

static PacketAction
handle_mobile_incoming(Connection &c, const void *data, size_t length)
{
    auto p = (const struct uo_packet_mobile_incoming *)data;

    if (length < sizeof(*p) - sizeof(p->items))
        return PacketAction::DISCONNECT;

    c.client.world.Apply(*p);
    return PacketAction::ACCEPT;
}

static PacketAction
handle_char_list(Connection &c, const void *data, size_t length)
{
    auto p = (const struct uo_packet_simple_character_list *)data;

    /* save character list */
    if (p->character_count > 0 &&
        length >= sizeof(*p) + (p->character_count - 1) * sizeof(p->character_info[0]))
        c.client.char_list = {p, length};

    /* respond directly during reconnect */
    if (c.client.reconnecting) {
        struct uo_packet_play_character p2 = {
            .cmd = PCK_PlayCharacter,
            .unknown0 = {},
            .name = {},
            .unknown1 = {},
            .flags = {},
            .unknown2 = {},
            .slot = c.character_index,
            .client_ip = 0xc0a80102, /* 192.168.1.2 */
        };

        LogFormat(2, "sending PlayCharacter\n");

        uo_client_send(c.client.client, &p2, sizeof(p2));

        return PacketAction::DROP;
    } else {
        for (auto &ls : c.servers) {
            if (ls.state == LinkedServer::State::GAME_LOGIN ||
                ls.state == LinkedServer::State::PLAY_SERVER) {
                uo_server_send(ls.server, data, length);
                ls.state = LinkedServer::State::CHAR_LIST;
            }
        }
        return PacketAction::DROP;
    }
}

static PacketAction
handle_account_login_reject(Connection &c, const void *data, [[maybe_unused]] size_t length)
{
    auto p = (const struct uo_packet_account_login_reject *)data;

    assert(length == sizeof(*p));

    if (c.client.reconnecting) {
        LogFormat(1, "reconnect failed: AccountLoginReject reason=0x%x\n",
                  p->reason);

        c.ScheduleReconnect();
        return PacketAction::DELETED;
    }

    if (c.IsInGame())
        return PacketAction::DISCONNECT;

    return PacketAction::ACCEPT;
}

static PacketAction
handle_relay(Connection &c, const void *data, [[maybe_unused]] size_t length)
{
    /* this packet tells the UO client where to connect; uoproxy hides
       this packet from the client, and only internally connects to
       the new server */
    auto p = (const struct uo_packet_relay *)data;
    struct uo_packet_relay relay;
    int ret;
    struct uo_packet_game_login login;

    assert(length == sizeof(*p));

    if (c.IsInGame() && !c.client.reconnecting)
        return PacketAction::DISCONNECT;

    LogFormat(2, "changing to game connection\n");

    /* save the relay packet - its buffer will be freed soon */
    relay = *p;

    /* close old connection */
    c.Disconnect();

    /* restore the "reconnecting" flag: it was cleared by
       connection_client_disconnect(), but since continue reconnecting
       on the new connection, we want to set it now.  the check at the
       start of this function ensures that c.in_game is only set
       during reconnecting. */

    c.client.reconnecting = c.IsInGame();

    /* extract new server's address */

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = relay.port.raw();
    sin.sin_addr.s_addr = relay.ip.raw();

    /* connect to new server */

    if (c.client_version.seed != nullptr)
        c.client_version.seed->seed = relay.auth_id;

    ret = c.Connect((const struct sockaddr *)&sin,
                     sizeof(sin), relay.auth_id);
    if (ret != 0) {
        log_error("connect to game server failed", ret);
        return PacketAction::DISCONNECT;
    }

    /* send game login to new server */
    LogFormat(2, "connected, doing GameLogin\n");

    login.cmd = PCK_GameLogin;
    login.auth_id = relay.auth_id;
    login.credentials = c.credentials;

    uo_client_send(c.client.client, &login, sizeof(login));

    return PacketAction::DELETED;
}

static PacketAction
handle_server_list(Connection &c, const void *data, size_t length)
{
    /* this packet tells the UO client where to connect; what
       we do here is replace the server IP with our own one */
    auto p = (const struct uo_packet_server_list *)data;

    (void)c;

    assert(length > 0);

    if (length < sizeof(*p) || p->unknown_0x5d != 0x5d)
        return PacketAction::DISCONNECT;

    if (c.instance.config.antispy)
        send_antispy(c.client.client);

    if (c.client.reconnecting) {
        struct uo_packet_play_server p2 = {
            .cmd = PCK_PlayServer,
            .index = 0, /* XXX */
        };

        uo_client_send(c.client.client, &p2, sizeof(p2));

        return PacketAction::DROP;
    }

    const unsigned count = p->num_game_servers;
    LogFormat(5, "serverlist: %u servers\n", count);

    const auto *server_info = p->game_servers;
    if (length != sizeof(*p) + (count - 1) * sizeof(*server_info))
        return PacketAction::DISCONNECT;

    for (unsigned i = 0; i < count; i++, server_info++) {
        const unsigned k = server_info->index;
        if (k != i)
            return PacketAction::DISCONNECT;

        LogFormat(6, "server %u: name=%s address=0x%08x\n",
                  (unsigned)server_info->index,
                  server_info->name,
                  (unsigned)server_info->address);
    }

    /* forward only to the clients which are waiting for the server
       list (should be only one) */
    for (auto &ls : c.servers) {
        if (ls.state == LinkedServer::State::ACCOUNT_LOGIN) {
            ls.state = LinkedServer::State::SERVER_LIST;
            uo_server_send(ls.server, data, length);
        }
    }

    return PacketAction::DROP;
}

static PacketAction
handle_speak_unicode(Connection &c, const void *, size_t)
{
    welcome(c);

    return PacketAction::ACCEPT;
}

static PacketAction
handle_supported_features(Connection &c, const void *data, size_t length)
{
    if (c.client_version.protocol >= PROTOCOL_6_0_14) {
        auto p = (const struct uo_packet_supported_features_6014 *)data;
        assert(length == sizeof(*p));

        c.client.supported_features_flags = p->flags;

        struct uo_packet_supported_features p6;
        supported_features_6014_to_6(&p6, p);
        c.BroadcastToInGameClientsDivert(PROTOCOL_6_0_14,
                                          &p6, sizeof(p6),
                                          data, length);
    } else {
        auto p = (const struct uo_packet_supported_features *)data;
        assert(length == sizeof(*p));

        c.client.supported_features_flags = p->flags;

        struct uo_packet_supported_features_6014 p7;
        supported_features_6_to_6014(&p7, p);
        c.BroadcastToInGameClientsDivert(PROTOCOL_6_0_14,
                                          data, length,
                                          &p7, sizeof(p7));
    }

    return PacketAction::DROP;
}

static PacketAction
handle_season(Connection &c, const void *data, [[maybe_unused]] size_t length)
{
    auto p = (const struct uo_packet_season *)data;

    assert(length == sizeof(*p));

    c.client.world.packet_season = *p;

    return PacketAction::ACCEPT;
}

static PacketAction
handle_client_version(Connection &c, const void *, size_t)
{
    if (c.client_version.IsDefined()) {
        LogFormat(3, "sending cached client version '%s'\n",
                  c.client_version.packet->version);

        /* respond to this packet directly if we know the version
           number */
        uo_client_send(c.client.client, c.client_version.packet.get(),
                       c.client_version.packet.size());
        return PacketAction::DROP;
    } else {
        /* we don't know the version - forward the request to all
           clients */
        c.client.version_requested = true;
        return PacketAction::ACCEPT;
    }
}

static PacketAction
handle_extended(Connection &c, const void *data, size_t length)
{
    auto p = (const struct uo_packet_extended *)data;

    if (length < sizeof(*p))
        return PacketAction::DISCONNECT;

    LogFormat(8, "from server: extended 0x%04x\n", (unsigned)p->extended_cmd);

    switch (p->extended_cmd) {
    case 0x0008:
        if (length <= sizeof(c.client.world.packet_map_change))
            memcpy(&c.client.world.packet_map_change, data, length);

        break;

    case 0x0018:
        if (length <= sizeof(c.client.world.packet_map_patches))
            memcpy(&c.client.world.packet_map_patches, data, length);
        break;
    }

    return PacketAction::ACCEPT;
}

static PacketAction
handle_world_item_7(Connection &c, const void *data, size_t length)
{
    auto p = (const struct uo_packet_world_item_7 *)data;
    assert(length == sizeof(*p));

    struct uo_packet_world_item old;
    world_item_from_7(&old, p);

    c.client.world.Apply(*p);

    c.BroadcastToInGameClientsDivert(PROTOCOL_7,
                                      &old, old.length,
                                      data, length);
    return PacketAction::DROP;
}

static PacketAction
handle_protocol_extension(Connection &c, const void *data, size_t length)
{
    auto p = (const struct uo_packet_protocol_extension *)data;
    if (length < sizeof(*p))
        return PacketAction::DISCONNECT;

    if (p->extension == 0xfe) {
        /* BeginRazorHandshake; fake a response to avoid getting
           kicked */

        struct uo_packet_protocol_extension response = {
            .cmd = PCK_ProtocolExtension,
            .length = sizeof(response),
            .extension = 0xff,
        };

        uo_client_send(c.client.client, &response, sizeof(response));
        return PacketAction::DROP;
    }

    return PacketAction::ACCEPT;
}

const struct client_packet_binding server_packet_bindings[] = {
    { PCK_MobileStatus, handle_mobile_status }, /* 0x11 */
    { PCK_WorldItem, handle_world_item }, /* 0x1a */
    { PCK_Start, handle_start }, /* 0x1b */
    { PCK_SpeakAscii, handle_speak_ascii }, /* 0x1c */
    { PCK_Delete, handle_delete }, /* 0x1d */
    { PCK_MobileUpdate, handle_mobile_update }, /* 0x20 */
    { PCK_WalkCancel, handle_walk_cancel }, /* 0x21 */
    { PCK_WalkAck, handle_walk_ack }, /* 0x22 */
    { PCK_ContainerOpen, handle_container_open }, /* 0x24 */
    { PCK_ContainerUpdate, handle_container_update }, /* 0x25 */
    { PCK_Equip, handle_equip }, /* 0x2e */
    { PCK_ContainerContent, handle_container_content }, /* 0x3c */
    { PCK_PersonalLightLevel, handle_personal_light_level }, /* 0x4e */
    { PCK_GlobalLightLevel, handle_global_light_level }, /* 0x4f */
    { PCK_PopupMessage, handle_popup_message }, /* 0x53 */
    { PCK_ReDrawAll, handle_login_complete }, /* 0x55 */
    { PCK_Target, handle_target }, /* 0x6c */
    { PCK_WarMode, handle_war_mode }, /* 0x72 */
    { PCK_Ping, handle_ping }, /* 0x73 */
    { PCK_ZoneChange, handle_zone_change }, /* 0x76 */
    { PCK_MobileMoving, handle_mobile_moving }, /* 0x77 */
    { PCK_MobileIncoming, handle_mobile_incoming }, /* 0x78 */
    { PCK_CharList3, handle_char_list }, /* 0x81 */
    { PCK_AccountLoginReject, handle_account_login_reject }, /* 0x82 */
    { PCK_CharList2, handle_char_list }, /* 0x86 */
    { PCK_Relay, handle_relay }, /* 0x8c */
    { PCK_ServerList, handle_server_list }, /* 0xa8 */
    { PCK_CharList, handle_char_list }, /* 0xa9 */
    { PCK_SpeakUnicode, handle_speak_unicode }, /* 0xae */
    { PCK_SupportedFeatures, handle_supported_features }, /* 0xb9 */
    { PCK_Season, handle_season }, /* 0xbc */
    { PCK_ClientVersion, handle_client_version }, /* 0xbd */
    { PCK_Extended, handle_extended }, /* 0xbf */
    { PCK_WorldItem7, handle_world_item_7 }, /* 0xf3 */
    { PCK_ProtocolExtension, handle_protocol_extension }, /* 0xf0 */
    {}
};
