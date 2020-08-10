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
#include "packets.h"
#include "VerifyPacket.hxx"
#include "Connection.hxx"
#include "LinkedServer.hxx"
#include "Server.hxx"
#include "Client.hxx"
#include "Config.hxx"
#include "Log.hxx"
#include "version.h"
#include "compiler.h"
#include "Bridge.hxx"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

static void welcome(Connection *c) {
    for (auto &ls : c->servers) {
        if (!ls.attaching && !ls.is_zombie && !ls.welcome) {
            uo_server_speak_console(ls.server, "Welcome to uoproxy v" VERSION "!  "
                                    "http://max.kellermann.name/projects/uoproxy/");
            ls.welcome = true;
        }
    }
}

/** send a HardwareInfo packet to the server, containing inane
    information, to overwrite its old database entry */
static void send_antispy(UO::Client *client) {
    struct uo_packet_hardware p;

    memset(&p, 0, sizeof(p));

    p = (struct uo_packet_hardware){
        .cmd = PCK_Hardware,
        .unknown0 = 2,
        .instance_id = htonl(0xdeadbeef),
        .os_major = htonl(5),
        .os_minor = 0,
        .os_revision = 0,
        .cpu_manufacturer = 3,
        .cpu_family = htonl(6),
        .cpu_model = htonl(8),
        .cpu_clock = htonl(997),
        .cpu_quantity = 8,
        .physical_memory = htonl(256),
        .screen_width = htonl(1600),
        .screen_height = htonl(1200),
        .screen_depth = htonl(32),
        .dx_major = htons(9),
        .dx_minor = htons(0),
        .vc_description = {
            'S', 0, '3', 0, ' ', 0, 'T', 0, 'r', 0, 'i', 0, 'o',
        },
        .vc_vendor_id = 0,
        .vc_device_id = 0,
        .vc_memory = htonl(4),
        .distribution = 2,
        .clients_running = 1,
        .clients_installed = 1,
        .partial_installed = 0,
        .language = { 'e', 0, 'n', 0, 'u', 0 },
    };

    uo_client_send(client, &p, sizeof(p));
}

static packet_action_t handle_mobile_status(Connection *c,
                                            const void *data, size_t length) {
    auto p = (const struct uo_packet_mobile_status *)data;

    (void)length;

    c->client.world.Apply(*p);
    return PA_ACCEPT;
}

static packet_action_t handle_world_item(Connection *c,
                                         const void *data, size_t length) {
    auto p = (const struct uo_packet_world_item *)data;

    assert(length <= sizeof(*p));

    c->client.world.Apply(*p);
    return PA_ACCEPT;
}

static packet_action_t handle_start(Connection *c,
                                    const void *data, size_t length) {
    auto p = (const struct uo_packet_start *)data;

    assert(length == sizeof(*p));

    c->client.world.packet_start = *p;
    c->in_game = true;

    /* if we're auto-reconnecting, this is the point where it
       succeeded */
    c->client.reconnecting = false;

    c->walk.seq_next = 0;

    return PA_ACCEPT;
}

static packet_action_t handle_speak_ascii(Connection *c,
                                          const void *data, size_t length) {
    (void)data;
    (void)length;

    welcome(c);

    return PA_ACCEPT;
}

static packet_action_t handle_delete(Connection *c,
                                     const void *data, size_t length) {
    auto p = (const struct uo_packet_delete *)data;

    assert(length == sizeof(*p));

    c->client.world.RemoveSerial(p->serial);
    return PA_ACCEPT;
}

static packet_action_t handle_mobile_update(Connection *c,
                                            const void *data, size_t length) {
    auto p = (const struct uo_packet_mobile_update *)data;

    assert(length == sizeof(*p));

    c->client.world.Apply(*p);
    return PA_ACCEPT;
}

static packet_action_t handle_walk_cancel(Connection *c,
                                          const void *data, size_t length) {
    auto p = (const struct uo_packet_walk_cancel *)data;

    assert(length == sizeof(*p));

    if (!c->in_game)
        return PA_DISCONNECT;

    connection_walk_cancel(c, p);

    return PA_DROP;
}

static packet_action_t handle_walk_ack(Connection *c,
                                       const void *data, size_t length) {
    auto p = (const struct uo_packet_walk_ack *)data;

    assert(length == sizeof(*p));

    connection_walk_ack(c, p);

    /* XXX: x/y/z etc. */

    return PA_DROP;
}

static packet_action_t handle_container_open(Connection *c,
                                             const void *data, size_t length) {
    if (client_version_defined(&c->client_version) &&
        c->client_version.protocol >= PROTOCOL_7) {
        auto p = (const struct uo_packet_container_open_7 *)data;

        c->client.world.Apply(*p);

        connection_broadcast_divert(c, PROTOCOL_7,
                                    &p->base, sizeof(p->base),
                                    data, length);
        return PA_DROP;
    } else {
        auto p = (const struct uo_packet_container_open *)data;
        assert(length == sizeof(*p));

        c->client.world.Apply(*p);

        const struct uo_packet_container_open_7 p7 = {
            .base = *p,
            .zero = 0x00,
            .x7d = 0x7d,
        };

        connection_broadcast_divert(c, PROTOCOL_7,
                                    p, sizeof(*p),
                                    &p7, sizeof(p7));

        return PA_DROP;
    }
}

static packet_action_t handle_container_update(Connection *c,
                                               const void *data, size_t length) {
    if (c->client_version.protocol < PROTOCOL_6) {
        auto p = (const struct uo_packet_container_update *)data;
        struct uo_packet_container_update_6 p6;

        assert(length == sizeof(*p));

        container_update_5_to_6(&p6, p);

        c->client.world.Apply(p6);

        connection_broadcast_divert(c, PROTOCOL_6,
                                    data, length,
                                    &p6, sizeof(p6));
    } else {
        auto p = (const struct uo_packet_container_update_6 *)data;
        struct uo_packet_container_update p5;

        assert(length == sizeof(*p));

        container_update_6_to_5(&p5, p);

        c->client.world.Apply(*p);

        connection_broadcast_divert(c, PROTOCOL_6,
                                    &p5, sizeof(p5),
                                    data, length);
    }

    return PA_DROP;
}

static packet_action_t handle_equip(Connection *c,
                                    const void *data, size_t length) {
    auto p = (const struct uo_packet_equip *)data;

    assert(length == sizeof(*p));

    c->client.world.Apply(*p);

    return PA_ACCEPT;
}

static packet_action_t handle_container_content(Connection *c,
                                                const void *data, size_t length) {
    if (packet_verify_container_content((const uo_packet_container_content *)data, length)) {
        /* protocol v5 */
        auto p = (const struct uo_packet_container_content *)data;
        struct uo_packet_container_content_6 *p6;
        size_t length6;

        p6 = container_content_5_to_6(p, &length6);
        if (p6 == nullptr) {
            log_oom();
            return PA_DROP;
        }

        c->client.world.Apply(*p6);

        connection_broadcast_divert(c, PROTOCOL_6,
                                    data, length,
                                    p6, length6);

        free(p6);
    } else if (packet_verify_container_content_6((const uo_packet_container_content_6 *)data, length)) {
        /* protocol v6 */
        auto p = (const struct uo_packet_container_content_6 *)data;
        struct uo_packet_container_content *p5;
        size_t length5;

        c->client.world.Apply(*p);

        p5 = container_content_6_to_5(p, &length5);
        if (p5 == nullptr) {
            log_oom();
            return PA_DROP;
        }

        connection_broadcast_divert(c, PROTOCOL_6,
                                    p5, length5,
                                    data, length);

        free(p5);
    } else
        return PA_DISCONNECT;

    return PA_DROP;
}

static packet_action_t handle_personal_light_level(Connection *c,
                                                   const void *data, size_t length) {
    auto p = (const struct uo_packet_personal_light_level *)data;

    assert(length == sizeof(*p));

    if (c->instance->config->light)
        return PA_DROP;

    if (c->client.world.packet_start.serial == p->serial)
        c->client.world.packet_personal_light_level = *p;

    return PA_ACCEPT;
}

static packet_action_t handle_global_light_level(Connection *c,
                                                 const void *data, size_t length) {
    auto p = (const struct uo_packet_global_light_level *)data;

    assert(length == sizeof(*p));

    if (c->instance->config->light)
        return PA_DROP;

    c->client.world.packet_global_light_level = *p;

    return PA_ACCEPT;
}

static packet_action_t handle_popup_message(Connection *c,
                                            const void *data, size_t length) {
    auto p = (const struct uo_packet_popup_message *)data;

    assert(length == sizeof(*p));

    if (c->client.reconnecting) {
        if (p->msg == 0x05) {
            connection_speak_console(c, "previous character is still online, trying again");
        } else {
            connection_speak_console(c, "character change failed, trying again");
        }

        connection_reconnect_delayed(c);
        return PA_DROP;
    }

    return PA_ACCEPT;
}

static packet_action_t handle_login_complete(Connection *c,
                                             const void *data, size_t length) {
    (void)data;
    (void)length;

    if (c->instance->config->antispy)
        send_antispy(c->client.client);

    return PA_ACCEPT;
}

static packet_action_t handle_target(Connection *c,
                                     const void *data, size_t length) {
    auto p = (const struct uo_packet_target *)data;

    assert(length == sizeof(*p));

    c->client.world.packet_target = *p;

    return PA_ACCEPT;
}

static packet_action_t handle_war_mode(Connection *c,
                                       const void *data, size_t length) {
    auto p = (const struct uo_packet_war_mode *)data;

    assert(length == sizeof(*p));

    c->client.world.packet_war_mode = *p;

    return PA_ACCEPT;
}

static packet_action_t handle_ping(Connection *c,
                                   const void *data, size_t length) {
    auto p = (const struct uo_packet_ping *)data;

    assert(length == sizeof(*p));

    c->client.ping_ack = p->id;

    return PA_DROP;
}

static packet_action_t handle_zone_change(Connection *c,
                                          const void *data, size_t length) {
    auto p = (const struct uo_packet_zone_change *)data;

    assert(length == sizeof(*p));

    c->client.world.Apply(*p);
    return PA_ACCEPT;
}

static packet_action_t handle_mobile_moving(Connection *c,
                                            const void *data, size_t length) {
    auto p = (const struct uo_packet_mobile_moving *)data;

    assert(length == sizeof(*p));

    c->client.world.Apply(*p);
    return PA_ACCEPT;
}

static packet_action_t handle_mobile_incoming(Connection *c,
                                              const void *data, size_t length) {
    auto p = (const struct uo_packet_mobile_incoming *)data;

    if (length < sizeof(*p) - sizeof(p->items))
        return PA_DISCONNECT;

    c->client.world.Apply(*p);
    return PA_ACCEPT;
}

static packet_action_t handle_char_list(Connection *c,
                                        const void *data, size_t length) {
    auto p = (const struct uo_packet_simple_character_list *)data;
    const void *data_end = ((const char*)data) + length;

    (void)data;
    (void)length;

    /* save character list */
    if (p->character_count > 0 && length >= sizeof(*p)) {
        unsigned idx;

        memset(c->client.characters, 0, sizeof(c->client.characters));

        for (idx = 0, c->client.num_characters = 0;
             idx < p->character_count &&
             idx < MAX_CHARACTERS &&
                 (const void*)&p->character_info[idx + 1] <= data_end;
             ++idx) {
            if (p->character_info[idx].name[0] != 0)
                ++c->client.num_characters;
        }

        memcpy(c->client.characters, p->character_info,
               idx * sizeof(c->client.characters[0]));
    }

    /* respond directly during reconnect */
    if (c->client.reconnecting) {
        struct uo_packet_play_character p2 = {
            .cmd = PCK_PlayCharacter,
            .slot = htonl(c->character_index),
            .client_ip = htonl(0xc0a80102), /* 192.168.1.2 */
        };

        LogFormat(2, "sending PlayCharacter\n");

        uo_client_send(c->client.client, &p2, sizeof(p2));

        return PA_DROP;
    } else if (c->instance->config->razor_workaround) {
        /* razor workaround -- we don't send the char list right away necessarily until they sent us GameLogin.. */
        for (auto &ls : c->servers) {
            if (ls.got_gamelogin && !ls.attaching && !ls.is_zombie)
                uo_server_send(ls.server, data, length);
            else {
                if (ls.enqueued_charlist) free(ls.enqueued_charlist);
                ls.enqueued_charlist = (struct uo_packet_simple_character_list *)calloc(1,length);
                if (ls.enqueued_charlist) {
                    memcpy(ls.enqueued_charlist, data, length);
                } else
                    log_oom();
            }
        }
        return PA_DROP;
    }

    return PA_ACCEPT;
}

static packet_action_t handle_account_login_reject(Connection *c,
                                                   const void *data, size_t length) {
    auto p = (const struct uo_packet_account_login_reject *)data;

    assert(length == sizeof(*p));

    if (c->client.reconnecting) {
        LogFormat(1, "reconnect failed: AccountLoginReject reason=0x%x\n",
                  p->reason);

        connection_reconnect_delayed(c);
        return PA_DROP;
    }

    if (c->in_game)
        return PA_DISCONNECT;

    return PA_ACCEPT;
}

static packet_action_t handle_relay(Connection *c,
                                    const void *data, size_t length) {
    /* this packet tells the UO client where to connect; uoproxy hides
       this packet from the client, and only internally connects to
       the new server */
    auto p = (const struct uo_packet_relay *)data;
    struct uo_packet_relay relay;
    int ret;
    struct uo_packet_game_login login;

    assert(length == sizeof(*p));

    if (c->in_game && !c->client.reconnecting)
        return PA_DISCONNECT;

    LogFormat(2, "changing to game connection\n");

    /* save the relay packet - its buffer will be freed soon */
    relay = *p;

    /* close old connection */
    connection_disconnect(c);

    /* restore the "reconnecting" flag: it was cleared by
       connection_client_disconnect(), but since continue reconnecting
       on the new connection, we want to set it now.  the check at the
       start of this function ensures that c->in_game is only set
       during reconnecting. */

    c->client.reconnecting = c->in_game;

    /* extract new server's address */

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = relay.port;
    sin.sin_addr.s_addr = relay.ip;

    /* connect to new server */

    if (c->client_version.seed != nullptr)
        c->client_version.seed->seed = relay.auth_id;

    ret = connection_client_connect(c, (const struct sockaddr *)&sin,
                                    sizeof(sin), relay.auth_id);
    if (ret != 0) {
        log_error("connect to game server failed", ret);
        return PA_DISCONNECT;
    }

    /* send game login to new server */
    LogFormat(2, "connected, doing GameLogin\n");

    login.cmd = PCK_GameLogin;
    login.auth_id = relay.auth_id;

    memcpy(login.username, c->username, sizeof(login.username));
    memcpy(login.password, c->password, sizeof(login.password));

    uo_client_send(c->client.client, &login, sizeof(login));

    return PA_DELETED;
}

static packet_action_t handle_server_list(Connection *c,
                                          const void *data, size_t length) {
    /* this packet tells the UO client where to connect; what
       we do here is replace the server IP with our own one */
    const unsigned char *p = (const unsigned char *)data;
    unsigned count, i, k;
    const struct uo_fragment_server_info *server_info;

    (void)c;

    assert(length > 0);

    if (length < 6 || p[3] != 0x5d)
        return PA_DISCONNECT;

    if (c->instance->config->antispy)
        send_antispy(c->client.client);

    if (c->client.reconnecting) {
        struct uo_packet_play_server p2 = {
            .cmd = PCK_PlayServer,
            .index = 0, /* XXX */
        };

        uo_client_send(c->client.client, &p2, sizeof(p2));

        return PA_DROP;
    }

    count = ntohs(*(const uint16_t*)(p + 4));
    LogFormat(5, "serverlist: %u servers\n", count);
    if (length != 6 + count * sizeof(*server_info))
        return PA_DISCONNECT;

    server_info = (const struct uo_fragment_server_info*)(p + 6);
    for (i = 0; i < count; i++, server_info++) {
        k = ntohs(server_info->index);
        if (k != i)
            return PA_DISCONNECT;

        LogFormat(6, "server %u: name=%s address=0x%08x\n",
                  ntohs(server_info->index),
                  server_info->name,
                  (unsigned)ntohl(server_info->address));
    }

    return PA_ACCEPT;
}

static packet_action_t handle_speak_unicode(Connection *c,
                                            const void *data, size_t length) {
    (void)data;
    (void)length;

    welcome(c);

    return PA_ACCEPT;
}

static packet_action_t handle_supported_features(Connection *c,
                                                 const void *data, size_t length) {
    if (c->client_version.protocol >= PROTOCOL_6_0_14) {
        auto p = (const struct uo_packet_supported_features_6014 *)data;
        assert(length == sizeof(*p));

        c->client.supported_features_flags = ntohl(p->flags);

        struct uo_packet_supported_features p6;
        supported_features_6014_to_6(&p6, p);
        connection_broadcast_divert(c, PROTOCOL_6_0_14,
                                    &p6, sizeof(p6),
                                    data, length);
    } else {
        auto p = (const struct uo_packet_supported_features *)data;
        assert(length == sizeof(*p));

        c->client.supported_features_flags = ntohs(p->flags);

        struct uo_packet_supported_features_6014 p7;
        supported_features_6_to_6014(&p7, p);
        connection_broadcast_divert(c, PROTOCOL_6_0_14,
                                    data, length,
                                    &p7, sizeof(p7));
    }

    return PA_DROP;
}

static packet_action_t handle_season(Connection *c,
                                     const void *data, size_t length) {
    auto p = (const struct uo_packet_season *)data;

    assert(length == sizeof(*p));

    c->client.world.packet_season = *p;

    return PA_ACCEPT;
}

static packet_action_t
handle_client_version(Connection *c,
                      const void *data __attr_unused,
                      size_t length __attr_unused) {
    if (client_version_defined(&c->client_version)) {
        LogFormat(3, "sending cached client version '%s'\n",
                  c->client_version.packet->version);

        /* respond to this packet directly if we know the version
           number */
        uo_client_send(c->client.client, c->client_version.packet,
                       c->client_version.packet_length);
        return PA_DROP;
    } else {
        /* we don't know the version - forward the request to all
           clients */
        c->client.version_requested = true;
        return PA_ACCEPT;
    }
}

static packet_action_t handle_extended(Connection *c,
                                       const void *data, size_t length) {
    auto p = (const struct uo_packet_extended *)data;

    if (length < sizeof(*p))
        return PA_DISCONNECT;

    LogFormat(8, "from server: extended 0x%04x\n", ntohs(p->extended_cmd));

    switch (ntohs(p->extended_cmd)) {
    case 0x0008:
        if (length <= sizeof(c->client.world.packet_map_change))
            memcpy(&c->client.world.packet_map_change, data, length);

        break;

    case 0x0018:
        if (length <= sizeof(c->client.world.packet_map_patches))
            memcpy(&c->client.world.packet_map_patches, data, length);
        break;
    }

    return PA_ACCEPT;
}

static packet_action_t
handle_world_item_7(Connection *c,
                    const void *data, size_t length)
{
    auto p = (const struct uo_packet_world_item_7 *)data;
    assert(length == sizeof(*p));

    struct uo_packet_world_item old;
    world_item_from_7(&old, p);

    c->client.world.Apply(*p);

    connection_broadcast_divert(c, PROTOCOL_7,
                                &old, ntohs(old.length),
                                data, length);
    return PA_DROP;
}

static packet_action_t
handle_protocol_extension(Connection *c,
                          const void *data, size_t length)
{
    auto p = (const struct uo_packet_protocol_extension *)data;
    if (length < sizeof(*p))
        return PA_DISCONNECT;

    if (p->extension == 0xfe) {
        /* BeginRazorHandshake; fake a response to avoid getting
           kicked */

        struct uo_packet_protocol_extension response = {
            .cmd = PCK_ProtocolExtension,
            .length = htons(sizeof(response)),
            .extension = 0xff,
        };

        uo_client_send(c->client.client, &response, sizeof(response));
        return PA_DROP;
    }

    return PA_ACCEPT;
}

struct client_packet_binding server_packet_bindings[] = {
    { .cmd = PCK_MobileStatus, /* 0x11 */
      .handler = handle_mobile_status,
    },
    { .cmd = PCK_WorldItem, /* 0x1a */
      .handler = handle_world_item,
    },
    { .cmd = PCK_Start, /* 0x1b */
      .handler = handle_start,
    },
    { .cmd = PCK_SpeakAscii, /* 0x1c */
      .handler = handle_speak_ascii,
    },
    { .cmd = PCK_Delete, /* 0x1d */
      .handler = handle_delete,
    },
    { .cmd = PCK_MobileUpdate, /* 0x20 */
      .handler = handle_mobile_update,
    },
    { .cmd = PCK_WalkCancel, /* 0x21 */
      .handler = handle_walk_cancel,
    },
    { .cmd = PCK_WalkAck, /* 0x22 */
      .handler = handle_walk_ack,
    },
    { .cmd = PCK_ContainerOpen, /* 0x24 */
      .handler = handle_container_open,
    },
    { .cmd = PCK_ContainerUpdate, /* 0x25 */
      .handler = handle_container_update,
    },
    { .cmd = PCK_Equip, /* 0x2e */
      .handler = handle_equip,
    },
    { .cmd = PCK_ContainerContent, /* 0x3c */
      .handler = handle_container_content,
    },
    { .cmd = PCK_PersonalLightLevel, /* 0x4e */
      .handler = handle_personal_light_level,
    },
    { .cmd = PCK_GlobalLightLevel, /* 0x4f */
      .handler = handle_global_light_level,
    },
    { .cmd = PCK_PopupMessage, /* 0x53 */
      .handler = handle_popup_message,
    },
    { .cmd = PCK_ReDrawAll, /* 0x55 */
      .handler = handle_login_complete,
    },
    { .cmd = PCK_Target, /* 0x6c */
      .handler = handle_target,
    },
    { .cmd = PCK_WarMode, /* 0x72 */
      .handler = handle_war_mode,
    },
    { .cmd = PCK_Ping, /* 0x73 */
      .handler = handle_ping,
    },
    { .cmd = PCK_ZoneChange, /* 0x76 */
      .handler = handle_zone_change,
    },
    { .cmd = PCK_MobileMoving, /* 0x77 */
      .handler = handle_mobile_moving,
    },
    { .cmd = PCK_MobileIncoming, /* 0x78 */
      .handler = handle_mobile_incoming,
    },
    { .cmd = PCK_CharList3, /* 0x81 */
      .handler = handle_char_list,
    },
    { .cmd = PCK_AccountLoginReject, /* 0x82 */
      .handler = handle_account_login_reject,
    },
    { .cmd = PCK_CharList2, /* 0x86 */
      .handler = handle_char_list,
    },
    { .cmd = PCK_Relay, /* 0x8c */
      .handler = handle_relay,
    },
    { .cmd = PCK_ServerList, /* 0xa8 */
      .handler = handle_server_list,
    },
    { .cmd = PCK_CharList, /* 0xa9 */
      .handler = handle_char_list,
    },
    { .cmd = PCK_SpeakUnicode, /* 0xae */
      .handler = handle_speak_unicode,
    },
    { .cmd = PCK_SupportedFeatures, /* 0xb9 */
      .handler = handle_supported_features,
    },
    { .cmd = PCK_Season, /* 0xbc */
      .handler = handle_season,
    },
    { .cmd = PCK_ClientVersion, /* 0xbd */
      .handler = handle_client_version,
    },
    { .cmd = PCK_Extended, /* 0xbf */
      .handler = handle_extended,
    },
    { .cmd = PCK_WorldItem7, /* 0xf3*/
      .handler = handle_world_item_7,
    },
    { .cmd = PCK_ProtocolExtension, /* 0xf0 */
      .handler = handle_protocol_extension,
    },
    { .handler = nullptr }
};
