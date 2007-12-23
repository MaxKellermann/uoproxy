/*
 * uoproxy
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

#include "log.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>

#include "instance.h"
#include "packets.h"
#include "handler.h"
#include "connection.h"
#include "client.h"
#include "server.h"
#include "config.h"

#define TALK_MAX 128

static char *simple_unicode_to_ascii(char *dest, const u_int16_t *src,
                                     size_t length) {
    size_t position;

    for (position = 0; position < length && src[position] != 0; position++) {
        u_int16_t ch = ntohs(src[position]);
        if (ch & 0xff00)
            return NULL;

        dest[position] = (char)ch;
    }

    dest[position] = 0;

    return dest;
}

static packet_action_t handle_talk(struct connection *c,
                                   const char *text) {
    /* the percent sign introduces an uoproxy command */
    if (text[0] == '%') {
        connection_handle_command(c, c->current_server, text + 1);
        return PA_DROP;
    }

    return PA_ACCEPT;
}

static packet_action_t handle_walk(struct connection *c,
                                   const void *data, size_t length) {
    const struct uo_packet_walk *p = data;

    assert(length == sizeof(*p));

    if (!c->in_game)
        return PA_DISCONNECT;

    if (c->reconnecting) {
        /* while reconnecting, reject all walk requests */
        struct uo_packet_walk_cancel p2 = {
            .cmd = PCK_WalkCancel,
            .seq = p->seq,
            .x = c->client.world.packet_start.x,
            .y = c->client.world.packet_start.y,
            .direction = c->client.world.packet_start.direction,
            .z = c->client.world.packet_start.z,
        };

        uo_server_send(c->current_server->server, &p2, sizeof(p2));

        return PA_DROP;
    }

    connection_walk_request(c, c->current_server, p);

    return PA_DROP;
}

static packet_action_t handle_talk_ascii(struct connection *c,
                                         const void *data, size_t length) {
    const struct uo_packet_talk_ascii *p = data;
    size_t text_length;

    (void)c;
    (void)data;
    (void)length;

    if (length < sizeof(*p))
        return PA_DISCONNECT;

    text_length = length - sizeof(*p);

    if (p->text[text_length] != 0)
        return PA_DISCONNECT;

    return handle_talk(c, p->text);
}

static packet_action_t handle_use(struct connection *c,
                                  const void *data, size_t length) {
    const struct uo_packet_use *p = data;

    assert(length == sizeof(*p));

    if (c->reconnecting) {
        uo_server_speak_console(c->current_server->server,
                                "please wait until uoproxy finishes reconnecting");
        return PA_DROP;
    }

#ifdef DUMP_USE
    do {
        struct item *i = connection_find_item(c, p->serial);
        if (i == NULL) {
            printf("Use 0x%x\n", ntohl(p->serial));
        } else {
            u_int16_t item_id;

            if (i->packet_world_item.cmd == PCK_WorldItem)
                item_id = i->packet_world_item.item_id;
            else if (i->packet_equip.cmd == PCK_Equip)
                item_id = i->packet_equip.item_id;
            else if (i->packet_container_update.cmd == PCK_ContainerUpdate)
                item_id = i->packet_container_update.item.item_id;
            else
                item_id = 0xffff;

            printf("Use 0x%x item_id=0x%x\n",
                   ntohl(p->serial), ntohs(item_id));
        }
        fflush(stdout);
    } while (0);
#endif

    return PA_ACCEPT;
}

static packet_action_t handle_action(struct connection *c,
                                     const void *data, size_t length) {
    (void)data;
    (void)length;

    if (c->reconnecting) {
        uo_server_speak_console(c->current_server->server,
                                "please wait until uoproxy finishes reconnecting");
        return PA_DROP;
    }

    return PA_ACCEPT;
}

static packet_action_t handle_lift_request(struct connection *c,
                                           const void *data, size_t length) {
    const struct uo_packet_lift_request *p = data;

    assert(length == sizeof(*p));

    if (c->reconnecting) {
        /* while reconnecting, reject all lift requests */
        struct uo_packet_lift_reject p2 = {
            .cmd = PCK_LiftReject,
            .reason = 0x00, /* CannotLift */
        };

        uo_server_send(c->current_server->server, &p2, sizeof(p2));

        return PA_DROP;
    }

    return PA_ACCEPT;
}

static packet_action_t handle_resynchronize(struct connection *c,
                                            const void *data, size_t length) {
    (void)c;
    (void)data;
    (void)length;

    if (verbose >= 3)
        printf("Resync!\n");

    c->walk.seq_next = 0;

    return PA_ACCEPT;
}

static packet_action_t handle_target(struct connection *c,
                                     const void *data, size_t length) {
    const struct uo_packet_target *p = data;

    assert(length == sizeof(*p));

    if (c->client.world.packet_target.cmd == PCK_Target &&
        c->client.world.packet_target.target_id != 0) {
        /* cancel this target for all other clients */
        memset(&c->client.world.packet_target, 0,
               sizeof(c->client.world.packet_target));
        c->client.world.packet_target.cmd = PCK_Target;
        c->client.world.packet_target.flags = 3;

        connection_broadcast_servers_except(c, &c->client.world.packet_target,
                                            sizeof(c->client.world.packet_target),
                                            c->current_server->server);
    }

    return PA_ACCEPT;
}

static packet_action_t handle_ping(struct connection *c,
                                   const void *data, size_t length) {
    uo_server_send(c->current_server->server, data, length);
    return PA_DROP;
}

static packet_action_t handle_account_login(struct connection *c,
                                            const void *data, size_t length) {
    const struct uo_packet_account_login *p = data;
    int ret;

    assert(length == sizeof(*p));
    assert(sizeof(p->username) == sizeof(c->username));
    assert(sizeof(p->password) == sizeof(c->password));

    if (c->in_game)
        return PA_DISCONNECT;

#ifdef DUMP_LOGIN
    printf("account_login: username=%s password=%s\n",
           p->username, p->password);
#endif

    if (c->client.client != NULL) {
        if (verbose >= 2)
            fprintf(stderr, "already logged in\n");
        return PA_DISCONNECT;
    }

    memcpy(c->username, p->username, sizeof(c->username));
    memcpy(c->password, p->password, sizeof(c->password));

    if (c->instance->config->login_address == NULL &&
        c->instance->config->game_servers != NULL &&
        c->instance->config->num_game_servers > 0) {
        unsigned i, num_game_servers = c->instance->config->num_game_servers;
        struct game_server_config *game_servers = c->instance->config->game_servers;
        struct uo_packet_server_list *p2;
        struct sockaddr_in *sin;

        length = sizeof(*p2) + (num_game_servers - 1) * sizeof(p2->game_servers[0]);

        p2 = calloc(1, length);
        if (p2 == NULL) {
            fprintf(stderr, "out of memory\n");
            return PA_DISCONNECT;
        }

        p2->cmd = PCK_ServerList;
        p2->length = htons(length);
        p2->unknown_0x5d = 0x5d;
        p2->num_game_servers = htons(num_game_servers);

        for (i = 0; i < num_game_servers; i++) {
            p2->game_servers[i].index = htons(i);
            snprintf(p2->game_servers[i].name, sizeof(p2->game_servers[i].name),
                     "%s", game_servers[i].name);

            if (game_servers[i].address->ai_family != AF_INET)
                continue;

            sin = (struct sockaddr_in*)game_servers[i].address->ai_addr;
            p2->game_servers[i].address = sin->sin_addr.s_addr;
        }

        uo_server_send(c->current_server->server, p2, length);
        free(p2);

        return PA_DROP;
    }

    ret = connection_client_connect(c, c->instance->config->login_address,
                                    uo_server_seed(c->current_server->server));
    if (ret != 0) {
        struct uo_packet_account_login_reject response;

        fprintf(stderr, "uo_client_create() failed: %s\n",
                strerror(-ret));

        response.cmd = PCK_AccountLoginReject;
        response.reason = 0x02; /* blocked */

        uo_server_send(c->current_server->server, &response,
                       sizeof(response));
        return PA_DROP;
    }

    return PA_ACCEPT;
}

static packet_action_t handle_game_login(struct connection *c,
                                         const void *data, size_t length) {
    const struct uo_packet_game_login *p = data;

    assert(length == sizeof(*p));
    assert(sizeof(p->username) == sizeof(c->username));
    assert(sizeof(p->password) == sizeof(c->password));

    /* valid uoproxy clients will never send this packet, as we're
       hiding the Relay packets from them */
    return PA_DISCONNECT;
}

static packet_action_t handle_play_character(struct connection *c,
                                             const void *data, size_t length) {
    const struct uo_packet_play_character *p = data;

    assert(length == sizeof(*p));

    if (c->current_server->attaching) {
        if (verbose >= 2)
            printf("attaching connection, stage II\n");
        attach_after_play_character(c, c->current_server);
        return PA_DROP;
    }

    c->character_index = ntohl(p->slot);

    return PA_ACCEPT;
}

static packet_action_t handle_play_server(struct connection *c,
                                          const void *data, size_t length) {
    const struct uo_packet_play_server *p = data;
    struct connection *c2;

    assert(length == sizeof(*p));

    if (c->in_game)
        return PA_DISCONNECT;

    assert(c->current_server->siblings.next == &c->servers);

    c->server_index = ntohs(p->index);

    c2 = find_attach_connection(c);
    if (c2 != NULL) {
        struct linked_server *ls = c->current_server;

        /* remove the object from the old connection */
        connection_server_remove(c, ls);
        connection_invalidate(c);

        /* attach it to the new connection */
        attach_after_play_server(c2, ls);

        return PA_DROP;
    }

    if (c->instance->config->login_address == NULL &&
        c->instance->config->game_servers != NULL &&
        c->instance->config->num_game_servers > 0) {
        unsigned i, num_game_servers = c->instance->config->num_game_servers;
        struct game_server_config *config;
        int ret;
        struct uo_packet_game_login login;

        assert(c->client.client == NULL);

        /* locate the selected game server */
        i = ntohs(p->index);
        if (i >= num_game_servers)
            return PA_DISCONNECT;

        config = c->instance->config->game_servers + i;

        /* connect to new server */
        ret = connection_client_connect(c, config->address, 0xdeadbeef);
        if (ret != 0) {
            if (verbose >= 1)
                fprintf(stderr, "connect to game server failed: %s\n",
                        strerror(-ret));
            return PA_DISCONNECT;
        }

        /* send game login to new server */
        login.cmd = PCK_GameLogin;
        login.auth_id = 0xdeadbeef;

        memcpy(login.username, c->username, sizeof(login.username));
        memcpy(login.password, c->password, sizeof(login.password));

        uo_client_send(c->client.client, &login, sizeof(login));

        return PA_DROP;
    }

    return PA_ACCEPT;
}

static packet_action_t handle_spy(struct connection *c,
                                  const void *data, size_t length) {
    (void)data;
    (void)length;

    if (c->instance->config->antispy)
        return PA_DROP;

    return PA_ACCEPT;
}

static packet_action_t handle_talk_unicode(struct connection *c,
                                           const void *data, size_t length) {
    const struct uo_packet_talk_unicode *p = data;

    if (length < sizeof(*p))
        return PA_DISCONNECT;

    if (p->type == 0xc0) {
        u_int16_t value = ntohs(p->text[0]);
        unsigned num_keywords = (value & 0xfff0) >> 4;
        unsigned skip_bits = (num_keywords + 1) * 12;
        unsigned skip_bytes = 12 + (skip_bits + 7) / 8;
        const char *start = data;
        const char *t = start + skip_bytes;
        size_t text_length = length - skip_bytes - 1;

        if (skip_bytes >= length)
            return PA_DISCONNECT;

        if (t[0] == 0 || t[text_length] != 0)
            return PA_DISCONNECT;

        /* the text may be UTF-8, but we ignore that for now */
        return handle_talk(c, t);
    } else {
        size_t text_length = (length - sizeof(*p)) / 2;

        if (text_length < TALK_MAX) { /* Regular */
            char msg[TALK_MAX], *t;

            t = simple_unicode_to_ascii(msg, p->text, text_length);
            if (t != NULL)
                return handle_talk(c, t);
        }
    }

    return PA_ACCEPT;
}

static packet_action_t handle_gump_response(struct connection *c,
                                            const void *data, size_t length) {
    const struct uo_packet_gump_response *p = data;
    struct uo_packet_close_gump close = {
        .cmd = PCK_Extended,
        .length = htons(sizeof(close)),
        .extended_cmd = htons(0x0004),
        .button_id = 0,
    };

    if (length < sizeof(*p))
        return PA_DISCONNECT;

    /* close the gump on all other clients */
    connection_broadcast_servers_except(c, &close, sizeof(close),
                                        c->current_server->server);

    return PA_ACCEPT;
}

static packet_action_t handle_client_version(struct connection *c,
                                             const void *data, size_t length) {
    (void)data;
    (void)length;

    if (c->instance->config->client_version != NULL) {
        struct uo_packet_client_version *p;
        size_t version_length;

        if (c->client.client == NULL || c->reconnecting)
            return PA_DROP;

        version_length = strlen(c->instance->config->client_version);

        p = malloc(sizeof(*p) + version_length);
        if (p == NULL)
            return PA_DROP;

        p->cmd = PCK_ClientVersion;
        p->length = htons(sizeof(*p) + version_length);
        memcpy(p->version, c->instance->config->client_version,
               version_length + 1);

        uo_client_send(c->client.client, p,
                       sizeof(*p) + version_length);

        free(p);

        return PA_DROP;
    } else if (c->client_version == NULL) {
        c->client_version = malloc(length);
        if (c->client_version != NULL)
            memcpy(c->client_version, data, length);
    }

    return PA_ACCEPT;
}

static packet_action_t handle_extended(struct connection *c,
                                       const void *data, size_t length) {
    const struct uo_packet_extended *p = data;

    (void)c;

    if (length < sizeof(*p))
        return PA_DISCONNECT;

#ifdef DUMP_HEADERS
    printf("from client: extended 0x%04x\n", ntohs(p->extended_cmd));
#endif

    return PA_ACCEPT;
}

static packet_action_t handle_hardware(struct connection *c,
                                       const void *data, size_t length) {
    (void)data;
    (void)length;

    if (c->instance->config->antispy)
        return PA_DROP;

    return PA_ACCEPT;
}

struct packet_binding client_packet_bindings[] = {
    { .cmd = PCK_Walk,
      .handler = handle_walk,
    },
    { .cmd = PCK_TalkAscii,
      .handler = handle_talk_ascii,
    },
    { .cmd = PCK_Use,
      .handler = handle_use,
    },
    { .cmd = PCK_Action,
      .handler = handle_action,
    },
    { .cmd = PCK_LiftRequest,
      .handler = handle_lift_request,
    },
    { .cmd = PCK_Resynchronize,
      .handler = handle_resynchronize,
    },
    { .cmd = PCK_Target, /* 0x6c */
      .handler = handle_target,
    },
    { .cmd = PCK_Ping,
      .handler = handle_ping,
    },
    { .cmd = PCK_AccountLogin,
      .handler = handle_account_login,
    },
    { .cmd = PCK_AccountLogin2,
      .handler = handle_account_login,
    },
    { .cmd = PCK_GameLogin,
      .handler = handle_game_login,
    },
    { .cmd = PCK_PlayCharacter,
      .handler = handle_play_character,
    },
    { .cmd = PCK_PlayServer,
      .handler = handle_play_server,
    },
    { .cmd = PCK_Spy, /* 0xa4 */
      .handler = handle_spy,
    },
    { .cmd = PCK_TalkUnicode,
      .handler = handle_talk_unicode,
    },
    { .cmd = PCK_GumpResponse,
      .handler = handle_gump_response,
    },
    { .cmd = PCK_ClientVersion, /* 0xbd */
      .handler = handle_client_version,
    },
    { .cmd = PCK_Extended,
      .handler = handle_extended,
    },
    { .cmd = PCK_Hardware, /* 0xd9 */
      .handler = handle_hardware,
    },
    { .handler = NULL }
};

