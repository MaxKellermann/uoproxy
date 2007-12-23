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

#include "instance.h"
#include "packets.h"
#include "handler.h"
#include "connection.h"
#include "client.h"
#include "server.h"
#include "config.h"
#include "log.h"
#include "compiler.h"

#include <sys/socket.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>

#define TALK_MAX 128

static char *simple_unicode_to_ascii(char *dest, const uint16_t *src,
                                     size_t length) {
    size_t position;

    for (position = 0; position < length && src[position] != 0; position++) {
        uint16_t ch = ntohs(src[position]);
        if (ch & 0xff00)
            return NULL;

        dest[position] = (char)ch;
    }

    dest[position] = 0;

    return dest;
}

static packet_action_t
handle_talk(struct linked_server *ls,
            const char *text)
{
    /* the percent sign introduces an uoproxy command */
    if (text[0] == '%') {
        connection_handle_command(ls, text + 1);
        return PA_DROP;
    }

    return PA_ACCEPT;
}

static packet_action_t
handle_walk(struct linked_server *ls,
            const void *data, size_t length)
{
    const struct uo_packet_walk *p = data;

    assert(length == sizeof(*p));

    if (!ls->connection->in_game)
        return PA_DISCONNECT;

    if (ls->connection->client.reconnecting) {
        struct world *world = &ls->connection->client.world;

        /* while reconnecting, reject all walk requests */
        struct uo_packet_walk_cancel p2 = {
            .cmd = PCK_WalkCancel,
            .seq = p->seq,
            .x = world->packet_start.x,
            .y = world->packet_start.y,
            .direction = world->packet_start.direction,
            .z = world->packet_start.z,
        };

        uo_server_send(ls->server, &p2, sizeof(p2));

        return PA_DROP;
    }

    connection_walk_request(ls, p);

    return PA_DROP;
}

static packet_action_t
handle_talk_ascii(struct linked_server *ls,
                  const void *data, size_t length)
{
    const struct uo_packet_talk_ascii *p = data;
    size_t text_length;

    if (length < sizeof(*p))
        return PA_DISCONNECT;

    text_length = length - sizeof(*p);

    if (p->text[text_length] != 0)
        return PA_DISCONNECT;

    return handle_talk(ls, p->text);
}

static packet_action_t
handle_use(struct linked_server *ls,
           const void *data, size_t length)
{
    const struct uo_packet_use *p = data;

    assert(length == sizeof(*p));

    if (ls->connection->client.reconnecting) {
        uo_server_speak_console(ls->server,
                                "please wait until uoproxy finishes reconnecting");
        return PA_DROP;
    }

#ifdef DUMP_USE
    do {
        struct item *i = connection_find_item(c, p->serial);
        if (i == NULL) {
            printf("Use 0x%x\n", ntohl(p->serial));
        } else {
            uint16_t item_id;

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

static packet_action_t
handle_action(struct linked_server *ls,
              const void *data __attr_unused, size_t length __attr_unused)
{
    if (ls->connection->client.reconnecting) {
        uo_server_speak_console(ls->server,
                                "please wait until uoproxy finishes reconnecting");
        return PA_DROP;
    }

    return PA_ACCEPT;
}

static packet_action_t
handle_lift_request(struct linked_server *ls,
                    const void *data, size_t length)
{
    const struct uo_packet_lift_request *p = data;

    assert(length == sizeof(*p));

    if (ls->connection->client.reconnecting) {
        /* while reconnecting, reject all lift requests */
        struct uo_packet_lift_reject p2 = {
            .cmd = PCK_LiftReject,
            .reason = 0x00, /* CannotLift */
        };

        uo_server_send(ls->server, &p2, sizeof(p2));

        return PA_DROP;
    }

    return PA_ACCEPT;
}

static packet_action_t
handle_resynchronize(struct linked_server *ls,
                     const void *data __attr_unused,
                     size_t length __attr_unused)
{
    if (verbose >= 3)
        printf("Resync!\n");

    ls->connection->walk.seq_next = 0;

    return PA_ACCEPT;
}

static packet_action_t
handle_target(struct linked_server *ls,
              const void *data, size_t length)
{
    const struct uo_packet_target *p = data;
    struct world *world = &ls->connection->client.world;

    assert(length == sizeof(*p));

    if (world->packet_target.cmd == PCK_Target &&
        world->packet_target.target_id != 0) {
        /* cancel this target for all other clients */
        memset(&world->packet_target, 0,
               sizeof(world->packet_target));
        world->packet_target.cmd = PCK_Target;
        world->packet_target.flags = 3;

        connection_broadcast_servers_except(ls->connection,
                                            &world->packet_target,
                                            sizeof(world->packet_target),
                                            ls->server);
    }

    return PA_ACCEPT;
}

static packet_action_t
handle_ping(struct linked_server *ls,
            const void *data, size_t length)
{
    uo_server_send(ls->server, data, length);
    return PA_DROP;
}

static packet_action_t
handle_account_login(struct linked_server *ls,
                     const void *data, size_t length) {
    const struct uo_packet_account_login *p = data;
    struct connection *c = ls->connection;
    const struct config *config = c->instance->config;
    int ret;

    assert(length == sizeof(*p));
    assert(sizeof(p->username) == sizeof(ls->connection->username));
    assert(sizeof(p->password) == sizeof(ls->connection->password));

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

    if (config->login_address == NULL &&
        config->game_servers != NULL &&
        config->num_game_servers > 0) {
        unsigned i, num_game_servers = config->num_game_servers;
        struct game_server_config *game_servers = config->game_servers;
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

        uo_server_send(ls->server, p2, length);
        free(p2);

        return PA_DROP;
    }

    ret = connection_client_connect(c, config->login_address,
                                    uo_server_seed(ls->server));
    if (ret != 0) {
        struct uo_packet_account_login_reject response;

        log_error("connection to login server failed", ret);

        response.cmd = PCK_AccountLoginReject;
        response.reason = 0x02; /* blocked */

        uo_server_send(ls->server, &response,
                       sizeof(response));
        return PA_DROP;
    }

    return PA_ACCEPT;
}

static packet_action_t
handle_game_login(struct linked_server *ls,
                  const void *data, size_t length)
{
    const struct uo_packet_game_login *p = data;

    assert(length == sizeof(*p));
    assert(sizeof(p->username) == sizeof(ls->connection->username));
    assert(sizeof(p->password) == sizeof(ls->connection->password));

    /* valid uoproxy clients will never send this packet, as we're
       hiding the Relay packets from them */
    return PA_DISCONNECT;
}

static packet_action_t
handle_play_character(struct linked_server *ls,
                      const void *data, size_t length)
{
    const struct uo_packet_play_character *p = data;

    assert(length == sizeof(*p));

    if (ls->attaching) {
        if (verbose >= 2)
            printf("attaching connection, stage II\n");
        attach_after_play_character(ls);
        return PA_DROP;
    }

    ls->connection->character_index = ntohl(p->slot);

    return PA_ACCEPT;
}

static packet_action_t
handle_play_server(struct linked_server *ls,
                   const void *data, size_t length)
{
    const struct uo_packet_play_server *p = data;
    struct connection *c = ls->connection, *c2;

    assert(length == sizeof(*p));

    if (c->in_game)
        return PA_DISCONNECT;

    assert(ls->siblings.next == &c->servers);

    c->server_index = ntohs(p->index);

    c2 = find_attach_connection(c);
    if (c2 != NULL) {
        /* remove the object from the old connection */
        connection_server_remove(c, ls);
        connection_delete(c);

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
            log_error("connect to game server failed", ret);
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

static packet_action_t
handle_spy(struct linked_server *ls,
           const void *data, size_t length)
{
    (void)data;
    (void)length;

    if (ls->connection->instance->config->antispy)
        return PA_DROP;

    return PA_ACCEPT;
}

static packet_action_t
handle_talk_unicode(struct linked_server *ls,
                    const void *data, size_t length)
{
    const struct uo_packet_talk_unicode *p = data;

    if (length < sizeof(*p))
        return PA_DISCONNECT;

    if (p->type == 0xc0) {
        uint16_t value = ntohs(p->text[0]);
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
        return handle_talk(ls, t);
    } else {
        size_t text_length = (length - sizeof(*p)) / 2;

        if (text_length < TALK_MAX) { /* Regular */
            char msg[TALK_MAX], *t;

            t = simple_unicode_to_ascii(msg, p->text, text_length);
            if (t != NULL)
                return handle_talk(ls, t);
        }
    }

    return PA_ACCEPT;
}

static packet_action_t
handle_gump_response(struct linked_server *ls,
                     const void *data, size_t length)
{
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
    connection_broadcast_servers_except(ls->connection, &close, sizeof(close),
                                        ls->server);

    return PA_ACCEPT;
}

static packet_action_t
handle_client_version(struct linked_server *ls,
                      const void *data, size_t length)
{
    struct connection *c = ls->connection;

    if (c->instance->config->client_version != NULL) {
        struct uo_packet_client_version *p;
        size_t version_length;

        if (c->client.client == NULL || c->client.reconnecting)
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

static packet_action_t
handle_extended(struct linked_server *ls __attr_unused,
                const void *data, size_t length)
{
    const struct uo_packet_extended *p = data;

    if (length < sizeof(*p))
        return PA_DISCONNECT;

#ifdef DUMP_HEADERS
    printf("from client: extended 0x%04x\n", ntohs(p->extended_cmd));
#endif

    return PA_ACCEPT;
}

static packet_action_t
handle_hardware(struct linked_server *ls,
                const void *data __attr_unused, size_t length __attr_unused)
{
    if (ls->connection->instance->config->antispy)
        return PA_DROP;

    return PA_ACCEPT;
}

struct server_packet_binding client_packet_bindings[] = {
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

