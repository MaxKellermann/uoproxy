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
#include "relay.h"
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
                                   void *data, size_t length) {
    const struct uo_packet_walk *p = data;

    assert(length == sizeof(*p));

    if (c->reconnecting) {
        /* while reconnecting, reject all walk requests */
        struct uo_packet_walk_cancel p2 = {
            .cmd = PCK_WalkCancel,
            .seq = p->seq,
            .x = c->packet_start.x,
            .y = c->packet_start.y,
            .direction = c->packet_start.direction,
            .z = c->packet_start.z,
        };

        uo_server_send(c->current_server->server, &p2, sizeof(p2));

        return PA_DROP;
    }

    return PA_ACCEPT;
}

static packet_action_t handle_talk_ascii(struct connection *c,
                                         void *data, size_t length) {
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
                                  void *data, size_t length) {
    (void)data;
    (void)length;

    if (c->reconnecting) {
        uo_server_speak_console(c->current_server->server,
                                "please wait until uoproxy finishes reconnecting");
        return PA_DROP;
    }

    return PA_ACCEPT;
}

static packet_action_t handle_action(struct connection *c,
                                     void *data, size_t length) {
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
                                           void *data, size_t length) {
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

static packet_action_t handle_ping(struct connection *c,
                                   void *data, size_t length) {
    uo_server_send(c->current_server->server, data, length);
    return PA_DROP;
}

static packet_action_t handle_account_login(struct connection *c,
                                            void *data, size_t length) {
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

    if (c->client != NULL) {
        fprintf(stderr, "already logged in\n");
        return PA_DISCONNECT;
    }

    ret = uo_client_create(c->instance->config->login_address,
                           uo_server_seed(c->current_server->server),
                           &c->client);
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

    memcpy(c->username, p->username, sizeof(c->username));
    memcpy(c->password, p->password, sizeof(c->password));

    return PA_ACCEPT;
}

static packet_action_t handle_game_login(struct connection *c,
                                         void *data, size_t length) {
    const struct uo_packet_game_login *p = data;
    int ret;
    const struct relay *relay;
    struct connection *c2;
    struct sockaddr_in sin;

    assert(length == sizeof(*p));
    assert(sizeof(p->username) == sizeof(c->username));
    assert(sizeof(p->password) == sizeof(c->password));

    if (c->in_game)
        return PA_DISCONNECT;

#ifdef DUMP_LOGIN
    printf("game_login: username=%s password=%s\n",
           p->username, p->password);
#endif

    if (c->client != NULL) {
        fprintf(stderr, "already logged in\n");
        return PA_DISCONNECT;
    }

    relay = relay_find(c->instance->relays, p->auth_id);
    if (relay == NULL) {
        fprintf(stderr, "invalid or expired auth_id: 0x%08x\n",
                p->auth_id);
        return PA_DISCONNECT;
    }

    for (c2 = c->instance->connections_head; c2 != NULL; c2 = c2->next) {
        if (c2 != c && c2->packet_start.serial != 0 &&
            memcmp(p->username, c2->username, sizeof(c2->username)) == 0 &&
            memcmp(p->password, c2->password, sizeof(c2->password)) == 0 &&
            c2->in_game) {
            struct uo_server *server = c->current_server->server;

            assert(server != NULL);

#ifdef DUMP_LOGIN
            printf("attaching connection\n");
#endif

            /* remove the object from the old connection */
            c->current_server->invalid = 1;
            c->current_server->server = NULL;

            /* attach it to the new connection */
            attach_after_game_login(c2, server);

            return PA_DISCONNECT;
        }
    }

    c->server_address = calloc(1, sizeof(*c->server_address));
    if (c->server_address == NULL) {
        fprintf(stderr, "out of memory");
        return PA_DISCONNECT;
    }

    sin.sin_family = AF_INET;
    sin.sin_port = relay->server_port;
    sin.sin_addr.s_addr = relay->server_ip;

    c->server_address->ai_family = AF_INET;
    c->server_address->ai_addrlen = sizeof(sin);
    c->server_address->ai_addr = malloc(sizeof(sin));

    if (c->server_address->ai_addr == NULL) {
        free(c->server_address);
        fprintf(stderr, "out of memory");
        return PA_DISCONNECT;
    }

    memcpy(c->server_address->ai_addr, &sin, sizeof(sin));

    ret = uo_client_create(c->server_address,
                           uo_server_seed(c->current_server->server),
                           &c->client);
    if (ret != 0) {
        fprintf(stderr, "uo_client_create() failed: %s\n",
                strerror(-ret));
        return PA_DISCONNECT;
    }

    memcpy(c->username, p->username, sizeof(c->username));
    memcpy(c->password, p->password, sizeof(c->password));

    return PA_ACCEPT;
}

static packet_action_t handle_play_character(struct connection *c,
                                             void *data, size_t length) {
    const struct uo_packet_play_character *p = data;

    assert(length == sizeof(*p));

    if (c->current_server->attaching) {
        printf("attaching connection, stage II\n");
        attach_after_play_character(c, c->current_server);
        return PA_DROP;
    }

    c->character_index = ntohl(p->slot);

    return PA_ACCEPT;
}

static packet_action_t handle_play_server(struct connection *c,
                                          void *data, size_t length) {
    const struct uo_packet_play_server *p = data;

    assert(length == sizeof(*p));

    if (c->current_server->attaching) {
        printf("attaching connection, stage II\n");
        attach_after_play_character(c, c->current_server);
        return PA_DROP;
    }

    if (c->instance->config->login_address == NULL &&
        c->instance->config->game_servers != NULL &&
        c->instance->config->num_game_servers > 0) {
        unsigned i, num_game_servers = c->instance->config->num_game_servers;
        struct game_server_config *config;
        struct sockaddr_in *sin;
        struct uo_packet_relay relay;

        i = ntohs(p->index);
        if (i >= num_game_servers)
            return PA_DISCONNECT;

        config = c->instance->config->game_servers + i;
        if (config->address->ai_family != AF_INET)
            return PA_DISCONNECT;

        sin = (struct sockaddr_in*)config->address->ai_addr;

        relay.cmd = PCK_Relay;
        relay.ip = sin->sin_addr.s_addr;
        relay.port = sin->sin_port;
        relay.auth_id = 0xdeadbeef; /* XXX */

        uo_server_send(c->current_server->server, &relay, sizeof(relay));

        return PA_DROP;
    }

    return PA_ACCEPT;
}

static packet_action_t handle_talk_unicode(struct connection *c,
                                           void *data, size_t length) {
    const struct uo_packet_talk_unicode *p = data;
    size_t text_length;

    if (length < sizeof(*p))
        return PA_DISCONNECT;

    text_length = (length - sizeof(*p)) / 2;

    if (p->type == 0x00 && text_length < TALK_MAX) { /* Regular */
        /* XXX this ignores MessageType.Encoded */
        char msg[TALK_MAX], *t;

        fflush(stdout);
        t = simple_unicode_to_ascii(msg, p->text, text_length);
        if (t != NULL)
            return handle_talk(c, t);
    }

    return PA_ACCEPT;
}

static packet_action_t handle_client_version(struct connection *c,
                                             void *data, size_t length) {
    (void)data;
    (void)length;

    if (c->instance->config->client_version != NULL) {
        struct uo_packet_client_version *p;
        size_t version_length;

        if (c->client == NULL || c->reconnecting)
            return PA_DROP;

        version_length = strlen(c->instance->config->client_version);

        p = malloc(sizeof(*p) + version_length);
        if (p == NULL)
            return PA_DROP;

        p->cmd = PCK_ClientVersion;
        p->length = htons(sizeof(*p) + version_length);
        memcpy(p->version, c->instance->config->client_version,
               version_length + 1);

        uo_client_send(c->client, p,
                       sizeof(*p) + version_length);

        free(p);

        return PA_DROP;
    }

    return PA_ACCEPT;
}

static packet_action_t handle_extended(struct connection *c,
                                       void *data, size_t length) {
    const struct uo_packet_extended *p = data;

    (void)c;

    if (length < sizeof(*p))
        return PA_DISCONNECT;

#ifdef DUMP_HEADERS
    printf("from client: extended 0x%04x\n", ntohs(p->extended_cmd));
#endif

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
    { .cmd = PCK_TalkUnicode,
      .handler = handle_talk_unicode,
    },
    { .cmd = PCK_ClientVersion,
      .handler = handle_client_version,
    },
    { .cmd = PCK_Extended,
      .handler = handle_extended,
    },
    { .handler = NULL }
};

