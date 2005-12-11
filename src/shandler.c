/*
 * uoproxy
 * $Id$
 *
 * (c) 2005 Max Kellermann <max@duempel.org>
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

#include <sys/types.h>
#include <assert.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "instance.h"
#include "packets.h"
#include "handler.h"
#include "relay.h"
#include "connection.h"
#include "server.h"
#include "client.h"

static void welcome(struct connection *c) {
    struct linked_server *ls;

    for (ls = c->servers_head; ls != NULL; ls = ls->next) {
        if (!ls->invalid && !ls->attaching && !ls->welcome) {
            uo_server_speak_console(ls->server, "Welcome to uoproxy v0.1.0!  "
                                    "http://max.kellermann.name/projects/uoproxy/");
            ls->welcome = 1;
        }
    }
}

static packet_action_t handle_mobile_status(struct connection *c,
                                            void *data, size_t length) {
    const struct uo_packet_mobile_status *p = data;

    (void)length;

    connection_mobile_status(c, p);

    return PA_ACCEPT;
}

static packet_action_t handle_world_item(struct connection *c,
                                         void *data, size_t length) {
    const struct uo_packet_world_item *p = data;

    assert(length <= sizeof(*p));

    connection_world_item(c, p);

    return PA_ACCEPT;
}

static packet_action_t handle_start(struct connection *c,
                                    void *data, size_t length) {
    struct uo_packet_start *p = data;

    assert(length == sizeof(*p));

    c->packet_start = *p;
    c->in_game = 1;

    /* if we're auto-reconnecting, this is the point where it
       succeeded */
    c->reconnecting = 0;

    c->walk.seq_next = 0;

    return PA_ACCEPT;
}

static packet_action_t handle_speak_ascii(struct connection *c,
                                          void *data, size_t length) {
    (void)data;
    (void)length;

    welcome(c);

    return PA_ACCEPT;
}

static packet_action_t handle_delete(struct connection *c,
                                     void *data, size_t length) {
    struct uo_packet_delete *p = data;

    assert(length == sizeof(*p));

    connection_remove_serial(c, p->serial);

    return PA_ACCEPT;
}

static packet_action_t handle_mobile_update(struct connection *c,
                                            void *data, size_t length) {
    struct uo_packet_mobile_update *p = data;

    assert(length == sizeof(*p));

    connection_mobile_update(c, p);

    return PA_ACCEPT;
}

static packet_action_t handle_walk_cancel(struct connection *c,
                                          void *data, size_t length) {
    struct uo_packet_walk_cancel *p = data;

    assert(length == sizeof(*p));

    if (!c->in_game)
        return PA_DISCONNECT;

    /* XXX: grab p->x/y/z etc. */

    connection_walk_cancel(c, p);

    return PA_DROP;
}

static packet_action_t handle_walk_ack(struct connection *c,
                                       void *data, size_t length) {
    struct uo_packet_walk_ack *p = data;

    assert(length == sizeof(*p));

    connection_walk_ack(c, p);

    /* XXX: x/y/z etc. */

    return PA_DROP;
}

static packet_action_t handle_personal_light_level(struct connection *c,
                                                   void *data, size_t length) {
    struct uo_packet_personal_light_level *p = data;

    assert(length == sizeof(*p));

    if (c->packet_start.serial == p->serial)
        c->packet_personal_light_level = *p;

    return PA_ACCEPT;
}

static packet_action_t handle_global_light_level(struct connection *c,
                                                 void *data, size_t length) {
    struct uo_packet_global_light_level *p = data;

    assert(length == sizeof(*p));

    c->packet_global_light_level = *p;

    return PA_ACCEPT;
}

static packet_action_t handle_popup_message(struct connection *c,
                                            void *data, size_t length) {
    const struct uo_packet_popup_message *p = data;

    assert(length == sizeof(*p));

    if (c->reconnecting) {
        if (p->msg == 0x05) {
            connection_speak_console(c, "previous character is still online, trying again");
        } else {
            connection_speak_console(c, "character change failed, trying again");
        }

        connection_reconnect(c);
        return PA_DROP;
    }

    return PA_ACCEPT;
}

static packet_action_t handle_war_mode(struct connection *c,
                                       void *data, size_t length) {
    const struct uo_packet_war_mode *p = data;

    assert(length == sizeof(*p));

    c->packet_war_mode = *p;

    return PA_ACCEPT;
}

static packet_action_t handle_ping(struct connection *c,
                                   void *data, size_t length) {
    struct uo_packet_ping *p = data;

    assert(length == sizeof(*p));

    c->ping_ack = p->id;

    return PA_DROP;
}

static packet_action_t handle_zone_change(struct connection *c,
                                          void *data, size_t length) {
    struct uo_packet_zone_change *p = data;

    assert(length == sizeof(*p));

    connection_mobile_zone(c, p);

    return PA_ACCEPT;
}

static packet_action_t handle_mobile_moving(struct connection *c,
                                            void *data, size_t length) {
    const struct uo_packet_mobile_moving *p = data;

    assert(length == sizeof(*p));

    connection_mobile_moving(c, p);

    return PA_ACCEPT;
}

static packet_action_t handle_mobile_incoming(struct connection *c,
                                              void *data, size_t length) {
    const struct uo_packet_mobile_incoming *p = data;

    (void)length;

    connection_mobile_incoming(c, p);

    return PA_ACCEPT;
}

static packet_action_t handle_char_list(struct connection *c,
                                        void *data, size_t length) {
    const struct uo_packet_simple_character_list *p = data;
    const void *data_end = ((const char*)data) + length;

    (void)data;
    (void)length;

    /* save character list */
    if (p->character_count > 0 && length >= sizeof(*p)) {
        unsigned idx;

        memset(c->characters, 0, sizeof(c->characters));

        for (idx = 0, c->num_characters = 0;
             idx < p->character_count &&
             idx < MAX_CHARACTERS &&
                 (const void*)&p->character_info[idx + 1] <= data_end;
             ++idx) {
            if (p->character_info[idx].name[0] != 0)
                ++c->num_characters;
        }

        memcpy(c->characters, p->character_info,
               idx * sizeof(c->characters[0]));
    }

    /* respond directly during reconnect */
    if (c->reconnecting) {
        struct uo_packet_play_character p2 = {
            .cmd = PCK_PlayCharacter,
            .slot = htonl(c->character_index),
            .client_ip = 0xdeadbeef, /* XXX */
        };

        printf("sending PlayCharacter\n");

        uo_client_send(c->client, &p2, sizeof(p2));

        return PA_DROP;
    }

    return PA_ACCEPT;
}

static packet_action_t handle_account_login_reject(struct connection *c,
                                                   void *data, size_t length) {
    struct uo_packet_account_login_reject *p = data;

    assert(length == sizeof(*p));

    if (c->in_game)
        return PA_DISCONNECT;

    if (c->reconnecting) {
        fprintf(stderr, "reconnect failed: AccountLoginReject reason=0x%x\n",
                p->reason);

        connection_reconnect(c);
        return PA_DROP;
    }

    return PA_ACCEPT;
}

static packet_action_t handle_relay(struct connection *c,
                                    void *data, size_t length) {
    /* this packet tells the UO client where to connect; what
       we do here is replace the server IP with our own one */
    struct uo_packet_relay *p = data;
    struct relay relay;
    int ret;
    struct sockaddr_in sin;
    socklen_t sin_len = sizeof(sin);
    struct uo_server *server;

    assert(length == sizeof(*p));

    if (c->reconnecting) {
        struct uo_packet_game_login p2 = {
            .cmd = PCK_GameLogin,
            .auth_id = p->auth_id,
        };

        printf("changing to game connection\n");

        uo_client_dispose(c->client);
        c->client = NULL;

        ret = uo_client_create(c->server_address, p->auth_id, &c->client);
        if (ret != 0) {
            fprintf(stderr, "reconnect failed: %s\n", strerror(-ret));
            return PA_DROP;
        }

        printf("connected, doing GameLogin\n");

        memcpy(p2.username, c->username, sizeof(p2.username));
        memcpy(p2.password, c->password, sizeof(p2.password));

        uo_client_send(c->client, &p2, sizeof(p2));

        return PA_DROP;
    }

    if (c->in_game)
        return PA_DISCONNECT;

    server = c->servers_head->server;

    /* remember the original IP/port */
    relay = (struct relay){
        .auth_id = p->auth_id,
        .server_ip = p->ip,
        .server_port = p->port,
    };

    relay_add(c->instance->relays, &relay);

    /* get our local address */
    ret = getsockname(uo_server_fileno(server),
                      (struct sockaddr*)&sin, &sin_len);
    if (ret < 0) {
        fprintf(stderr, "getsockname() failed: %s\n",
                strerror(errno));
        return PA_DISCONNECT;
    }

    if (sin.sin_family != AF_INET) {
        fprintf(stderr, "not AF_INET\n");
        return PA_DISCONNECT;
    }

    /* now overwrite the packet */
    p->ip = sin.sin_addr.s_addr;
    p->port = sin.sin_port;

    return PA_ACCEPT;
}

static packet_action_t handle_server_list(struct connection *c,
                                          void *data, size_t length) {
    /* this packet tells the UO client where to connect; what
       we do here is replace the server IP with our own one */
    unsigned char *p = data;
    unsigned count, i, k;
    struct uo_fragment_server_info *server_info;

    (void)c;

    assert(length > 0);

    if (length < 6 || p[3] != 0x5d)
        return PA_DISCONNECT;

    if (c->reconnecting) {
        struct uo_packet_play_server p2 = {
            .cmd = PCK_PlayServer,
            .index = 0, /* XXX */
        };

        uo_client_send(c->client, &p2, sizeof(p2));

        return PA_DROP;
    }

    count = ntohs(*(uint16_t*)(p + 4));
#ifdef DUMP_LOGIN
    printf("serverlist: %u servers\n", count);
#endif
    if (length != 6 + count * sizeof(*server_info))
        return PA_DISCONNECT;

    server_info = (struct uo_fragment_server_info*)(p + 6);
    for (i = 0; i < count; i++, server_info++) {
        k = ntohs(server_info->index);
        if (k != i)
            return PA_DISCONNECT;

#ifdef DUMP_LOGIN
        printf("server %u: name=%s address=0x%08x\n",
               ntohs(server_info->index),
               server_info->name,
               ntohl(server_info->address));
#endif
    }

    return PA_ACCEPT;
}

static packet_action_t handle_speak_unicode(struct connection *c,
                                            void *data, size_t length) {
    (void)data;
    (void)length;

    welcome(c);

    return PA_ACCEPT;
}

static packet_action_t handle_supported_features(struct connection *c,
                                                 void *data, size_t length) {
    struct uo_packet_supported_features *p = data;

    assert(length == sizeof(*p));

    c->supported_features_flags = p->flags;

    return PA_ACCEPT;
}

static packet_action_t handle_season(struct connection *c,
                                     void *data, size_t length) {
    const struct uo_packet_season *p = data;

    assert(length == sizeof(*p));

    c->packet_season = *p;

    return PA_ACCEPT;
}

static packet_action_t handle_extended(struct connection *c,
                                       void *data, size_t length) {
    const struct uo_packet_extended *p = data;

    if (length < sizeof(*p))
        return PA_DISCONNECT;

#ifdef DUMP_HEADERS
    printf("from server: extended 0x%04x\n", ntohs(p->extended_cmd));
#endif

    switch (ntohs(p->extended_cmd)) {
    case 0x0008:
        if (length <= sizeof(c->packet_map_change))
            memcpy(&c->packet_map_change, data, length);

        break;

    case 0x0018:
        if (length <= sizeof(c->packet_map_patches))
            memcpy(&c->packet_map_patches, data, length);
        break;
    }

    return PA_ACCEPT;
}

struct packet_binding server_packet_bindings[] = {
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
    { .cmd = PCK_PersonalLightLevel, /* 0x4e */
      .handler = handle_personal_light_level,
    },
    { .cmd = PCK_GlobalLightLevel, /* 0x4f */
      .handler = handle_global_light_level,
    },
    { .cmd = PCK_PopupMessage, /* 0x53 */
      .handler = handle_popup_message,
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
    { .cmd = PCK_Extended, /* 0xbf */
      .handler = handle_extended,
    },
    { .handler = NULL }
};
