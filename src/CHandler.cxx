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

#include "Instance.hxx"
#include "packets.h"
#include "Handler.hxx"
#include "CVersion.hxx"
#include "Connection.hxx"
#include "LinkedServer.hxx"
#include "Client.hxx"
#include "Server.hxx"
#include "Config.hxx"
#include "Log.hxx"
#include "compiler.h"
#include "Bridge.hxx"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif

#include <time.h>

#define TALK_MAX 128

static char *simple_unicode_to_ascii(char *dest, const uint16_t *src,
                                     size_t length) {
    size_t position;

    for (position = 0; position < length && src[position] != 0; position++) {
        uint16_t ch = ntohs(src[position]);
        if (ch & 0xff00)
            return nullptr;

        dest[position] = (char)ch;
    }

    dest[position] = 0;

    return dest;
}

static packet_action_t
handle_talk(LinkedServer *ls,
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
handle_create_character(LinkedServer *ls,
                        const void *data, size_t length)
{
    auto p = (const struct uo_packet_create_character *)data;
    assert(length == sizeof(*p));

    if (ls->connection->instance->config->antispy) {
        struct uo_packet_create_character q = *p;
        q.client_ip = htonl(0xc0a80102);
        uo_client_send(ls->connection->client.client, &q, sizeof(q));
        return PA_DROP;
    } else
        return PA_ACCEPT;
}

static packet_action_t
handle_walk(LinkedServer *ls,
            const void *data, size_t length)
{
    auto p = (const struct uo_packet_walk *)data;

    assert(length == sizeof(*p));

    if (!ls->connection->in_game)
        return PA_DISCONNECT;

    if (ls->connection->client.reconnecting) {
        World *world = &ls->connection->client.world;

        /* while reconnecting, reject all walk requests */
        struct uo_packet_walk_cancel p2 = {
            .cmd = PCK_WalkCancel,
            .seq = p->seq,
            .x = world->packet_start.x,
            .y = world->packet_start.y,
            .direction = world->packet_start.direction,
            .z = (int8_t)world->packet_start.z,
        };

        uo_server_send(ls->server, &p2, sizeof(p2));

        return PA_DROP;
    }

    connection_walk_request(ls, p);

    return PA_DROP;
}

static packet_action_t
handle_talk_ascii(LinkedServer *ls,
                  const void *data, size_t length)
{
    auto p = (const struct uo_packet_talk_ascii *)data;

    if (length < sizeof(*p))
        return PA_DISCONNECT;

    const size_t text_length = length - sizeof(*p);
    if (p->text[text_length] != 0)
        return PA_DISCONNECT;

    return handle_talk(ls, p->text);
}

static packet_action_t
handle_use(LinkedServer *ls,
           const void *data, size_t length)
{
    auto p = (const struct uo_packet_use *)data;

    assert(length == sizeof(*p));

    if (ls->connection->client.reconnecting) {
        uo_server_speak_console(ls->server,
                                "please wait until uoproxy finishes reconnecting");
        return PA_DROP;
    }

#ifdef DUMP_USE
    do {
        Item *i = connection_find_item(c, p->serial);
        if (i == nullptr) {
            LogFormat(7, "Use 0x%x\n", ntohl(p->serial));
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

            LogFormat(7, "Use 0x%x item_id=0x%x\n",
                      ntohl(p->serial), ntohs(item_id));
        }
        fflush(stdout);
    } while (0);
#endif

    return PA_ACCEPT;
}

static packet_action_t
handle_action(LinkedServer *ls,
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
handle_lift_request(LinkedServer *ls,
                    const void *data, size_t length)
{
    auto p = (const struct uo_packet_lift_request *)data;

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
handle_drop(LinkedServer *ls,
            const void *data, size_t length)
{
    auto *client = &ls->connection->client;

    if (!ls->connection->in_game || client->reconnecting ||
        client->client == nullptr)
        return PA_DROP;

    if (ls->client_version.protocol < PROTOCOL_6) {
        auto p = (const struct uo_packet_drop *)data;

        assert(length == sizeof(*p));

        if (ls->connection->client_version.protocol < PROTOCOL_6)
            return PA_ACCEPT;

        struct uo_packet_drop_6 p6;
        drop_5_to_6(&p6, p);
        uo_client_send(client->client, &p6, sizeof(p6));
    } else {
        auto p = (const struct uo_packet_drop_6 *)data;

        assert(length == sizeof(*p));

        if (ls->connection->client_version.protocol >= PROTOCOL_6)
            return PA_ACCEPT;

        struct uo_packet_drop p5;
        drop_6_to_5(&p5, p);
        uo_client_send(client->client, &p5, sizeof(p5));
    }

    return PA_DROP;
}

static packet_action_t
handle_resynchronize(LinkedServer *ls,
                     const void *data __attr_unused,
                     size_t length __attr_unused)
{
    LogFormat(3, "Resync!\n");

    ls->connection->walk.seq_next = 0;

    return PA_ACCEPT;
}

static packet_action_t
handle_target(LinkedServer *ls,
              const void *data, size_t length)
{
    auto p = (const struct uo_packet_target *)data;
    World *world = &ls->connection->client.world;

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
handle_ping(LinkedServer *ls,
            const void *data, size_t length)
{
    uo_server_send(ls->server, data, length);
    return PA_DROP;
}

static packet_action_t
handle_account_login(LinkedServer *ls,
                     const void *data, size_t length)
{
    auto p = (const struct uo_packet_account_login *)data;
    Connection *c = ls->connection;
    const Config *config = c->instance->config;

    assert(length == sizeof(*p));
    assert(sizeof(p->username) == sizeof(ls->connection->username));
    assert(sizeof(p->password) == sizeof(ls->connection->password));

    if (c->in_game)
        return PA_DISCONNECT;

#ifdef DUMP_LOGIN
    LogFormat(7, "account_login: username=%s password=%s\n",
              p->username, p->password);
#endif

    if (c->client.client != nullptr) {
        LogFormat(2, "already logged in\n");
        return PA_DISCONNECT;
    }

    memcpy(c->username, p->username, sizeof(c->username));
    memcpy(c->password, p->password, sizeof(c->password));

    Connection *other = find_attach_connection(c);
    if (other != nullptr) {
        /* attaching to an existing connection, fake the server
           list */
        struct uo_packet_server_list p2;
        memset(&p2, 0, sizeof(p2));

        p2.cmd = PCK_ServerList;
        p2.length = htons(sizeof(p2));
        p2.unknown_0x5d = 0x5d;
        p2.num_game_servers = htons(1);

        p2.game_servers[0].index = htons(0);
        strcpy(p2.game_servers[0].name, "attach");
        p2.game_servers[0].address = 0xdeadbeef;

        uo_server_send(ls->server, &p2, sizeof(p2));
        return PA_DROP;
    }

    if (config->num_game_servers > 0) {
        /* we have a game server list and thus we emulate the login
           server */
        unsigned i, num_game_servers = config->num_game_servers;
        struct game_server_config *game_servers = config->game_servers;
        struct sockaddr_in *sin;

        assert(config->game_servers != nullptr);

        struct uo_packet_server_list *p2;
        length = sizeof(*p2) + (num_game_servers - 1) * sizeof(p2->game_servers[0]);

        p2 = (struct uo_packet_server_list *)calloc(1, length);
        if (p2 == nullptr) {
            log_oom();
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
    } else if (config->login_address != nullptr) {
        /* connect to the real login server */
        uint32_t seed;

        if (config->antispy)
            /* since the login server seed usually contains the
               internal IP address of the client, we want to hide it
               in antispy mode - always send 192.168.1.2 which is
               generic enough to be useless */
            seed = htonl(0xc0a80102);
        else
            seed = uo_server_seed(ls->server);

        int ret = connection_client_connect(c, config->login_address->ai_addr,
                                            config->login_address->ai_addrlen,
                                            seed);
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
    } else {
        /* should not happen */

        return PA_DISCONNECT;
    }
}

static packet_action_t
handle_game_login(LinkedServer *ls,
                  const void *data, size_t length)
{
    auto p = (const struct uo_packet_game_login *)data;

    assert(length == sizeof(*p));
    assert(sizeof(p->username) == sizeof(ls->connection->username));
    assert(sizeof(p->password) == sizeof(ls->connection->password));

    if (ls->connection->instance->config->razor_workaround) {
        bool was_attach = false;

        /* I have observed the Razor client ignoring the redirect if the IP
           address differs from what it connected to.  (I guess this is a bug in
           RunUO & Razor).  In that case it does a gamelogin on the old
           linked_server without reconnecting to us.

           So we apply the zombie-lookup only if the remote UO client actually
           did bother to reconnet to us. */
        if (!ls->connection->client.client) {
            Connection *reuse_conn = nullptr;
            Instance *instance = ls->connection->instance;

            /* this should only happen in redirect mode.. so look for the
               correct zombie so that we can re-use its connection to the UO
               server. */
            for (auto &c : instance->connections) {
                for (auto &ls2 : c.servers) {
                    if (ls2.is_zombie
                        && ls2.auth_id == p->auth_id
                        && !strcmp(c.username, p->username)
                        && !strcmp(c.password, p->password)) {
                        /* found it! Eureka! */
                        reuse_conn = &c;
                        ls2.expecting_reconnect = false;
                        // copy over charlist (if any)..
                        ls->enqueued_charlist = ls2.enqueued_charlist;
                        ls2.enqueued_charlist = nullptr;
                        was_attach = ls2.attaching;
                        ls2.attaching = false;
                        break;
                    }
                }
                if (reuse_conn) break;
            }
            if (reuse_conn) {
                Connection *c = ls->connection;

                /* copy the previously detected protocol version */
                if (!was_attach)
                    reuse_conn->client_version.protocol = c->client_version.protocol;

                /* remove the object from the old connection */
                connection_server_remove(c, ls);
                connection_delete(c);

                LogFormat(2, "attaching redirected client to its previous connection\n");

                connection_server_add(reuse_conn, ls);
            } else {
                /* houston, we have a problem -- reject the game login -- it
                   either came in too slowly (and so we already reaped the
                   zombie) or it was a hack attempt (wrong password) */
                LogFormat(2, "could not find previous connection for redirected client"
                          " -- disconnecting client!\n");
                return PA_DISCONNECT;
            }
        } else
            was_attach = ls->attaching;
        /* after GameLogin, must enable compression. */
        uo_server_set_compression(ls->server, true);
        ls->got_gamelogin = true;
        ls->attaching = false;
        if (ls->connection->in_game && was_attach) {
            /* already in game .. this was likely an attach connection */
            attach_send_world(ls);
        } else if (ls->enqueued_charlist) {
            uo_server_send(ls->server, ls->enqueued_charlist,
                           htons(ls->enqueued_charlist->length));
        }
        free(ls->enqueued_charlist);
        ls->enqueued_charlist = nullptr;
        ls->expecting_reconnect = false;
        return PA_DROP;
    }

    /* Unless we're in razor workaround mode, valid UO clients will never send
       this packet since we're hiding redirects from them. */
    return PA_DISCONNECT;
}

static packet_action_t
handle_play_character(LinkedServer *ls,
                      const void *data, size_t length)
{
    auto p = (const struct uo_packet_play_character *)data;

    assert(length == sizeof(*p));

    ls->connection->character_index = ntohl(p->slot);

    return PA_ACCEPT;
}

static void
redirect_to_self(LinkedServer *ls, Connection *c __attr_unused)
{
    struct uo_packet_relay relay;
    static uint32_t authid = 0;
    struct in_addr addr;

    if (!authid) authid = time(0);

    relay.cmd = PCK_Relay;
    relay.port = uo_server_getsockport(ls->server);
    relay.ip = uo_server_getsockname(ls->server);
    addr.s_addr = relay.ip;
    LogFormat(8, "redirecting to: %s:%hu\n", inet_ntoa(addr), htons(relay.port));;
    relay.auth_id = ls->auth_id = authid++;
    ls->expecting_reconnect = true;
    uo_server_send(ls->server, &relay, sizeof(relay));
    return;
}

static packet_action_t
handle_play_server(LinkedServer *ls,
                   const void *data, size_t length)
{
    auto p = (const struct uo_packet_play_server *)data;
    Connection *c = ls->connection, *c2;
    packet_action_t retaction = PA_DROP;

    assert(length == sizeof(*p));

    if (c->in_game)
        return PA_DISCONNECT;

    assert(std::next(c->servers.iterator_to(*ls)) == c->servers.end());

    c->server_index = ntohs(p->index);

    c2 = find_attach_connection(c);
    if (c2 != nullptr) {
        /* remove the object from the old connection */
        connection_server_remove(c, ls);
        connection_delete(c);

        if (c2->instance->config->razor_workaround) { ///< need to send redirect
            /* attach it to the new connection and send redirect (below) */
            ls->attaching = true;
            connection_server_add(c2, ls);
        }  else {
            /* attach it to the new connection and begin playing right away */
            attach_after_play_server(c2, ls);
        }

        retaction = PA_DROP;
        c = c2;
    } else if (c->instance->config->login_address == nullptr &&
               c->instance->config->game_servers != nullptr &&
               c->instance->config->num_game_servers > 0) {
        unsigned i, num_game_servers = c->instance->config->num_game_servers;
        struct game_server_config *config;
        int ret;
        struct uo_packet_game_login login;
        uint32_t seed;

        assert(c->client.client == nullptr);

        /* locate the selected game server */
        i = ntohs(p->index);
        if (i >= num_game_servers)
            return PA_DISCONNECT;

        config = c->instance->config->game_servers + i;

        /* connect to new server */

        if (c->client_version.seed != nullptr)
            seed = c->client_version.seed->seed;
        else
            seed = htonl(0xc0a80102); /* 192.168.1.2 */

        ret = connection_client_connect(c, config->address->ai_addr,
                                        config->address->ai_addrlen, seed);
        if (ret != 0) {
            log_error("connect to game server failed", ret);
            return PA_DISCONNECT;
        }

        /* send game login to new server */
        login.cmd = PCK_GameLogin;
        login.auth_id = seed;

        memcpy(login.username, c->username, sizeof(login.username));
        memcpy(login.password, c->password, sizeof(login.password));

        uo_client_send(c->client.client, &login, sizeof(login));

        retaction = PA_DROP;
    } else
        retaction = PA_ACCEPT;

    if (c->instance->config->razor_workaround) {
        /* Razor workaround -> send the redirect packet to the client and tell
           them to redirect to self!  This is because Razor refuses to work if
           it doesn't see a redirect packet.  Note that after the redirect,
           the client immediately sends 'GameLogin' which means we turn
           compression on. */
        redirect_to_self(ls, c);
    }

    return retaction;
}

static packet_action_t
handle_spy(LinkedServer *ls,
           const void *data, size_t length)
{
    (void)data;
    (void)length;

    if (ls->connection->instance->config->antispy)
        return PA_DROP;

    return PA_ACCEPT;
}

static packet_action_t
handle_talk_unicode(LinkedServer *ls,
                    const void *data, size_t length)
{
    auto p = (const struct uo_packet_talk_unicode *)data;

    if (length < sizeof(*p))
        return PA_DISCONNECT;

    if (p->type == 0xc0) {
        uint16_t value = ntohs(p->text[0]);
        unsigned num_keywords = (value & 0xfff0) >> 4;
        unsigned skip_bits = (num_keywords + 1) * 12;
        unsigned skip_bytes = 12 + (skip_bits + 7) / 8;
        const char *start = (const char *)data;
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
            if (t != nullptr)
                return handle_talk(ls, t);
        }
    }

    return PA_ACCEPT;
}

static packet_action_t
handle_gump_response(LinkedServer *ls,
                     const void *data, size_t length)
{
    auto p = (const struct uo_packet_gump_response *)data;

    if (length < sizeof(*p))
        return PA_DISCONNECT;

    /* close the gump on all other clients */
    const struct uo_packet_close_gump close = {
        .cmd = PCK_Extended,
        .length = htons(sizeof(close)),
        .extended_cmd = htons(0x0004),
        .type_id = p->type_id,
        .button_id = 0,
    };

    connection_broadcast_servers_except(ls->connection, &close, sizeof(close),
                                        ls->server);

    return PA_ACCEPT;
}

static packet_action_t
handle_client_version(LinkedServer *ls,
                      const void *data, size_t length)
{
    Connection *c = ls->connection;
    auto p = (const struct uo_packet_client_version *)data;

    if (!client_version_defined(&ls->client_version)) {
        bool was_unkown = ls->client_version.protocol == PROTOCOL_UNKNOWN;
        int ret = client_version_copy(&ls->client_version, p, length);
        if (ret > 0) {
            if (was_unkown)
                uo_server_set_protocol(ls->server,
                                       ls->client_version.protocol);

            LogFormat(2, "client version '%s', protocol '%s'\n",
                      ls->client_version.packet->version,
                      protocol_name(ls->client_version.protocol));
        }
    }

    if (client_version_defined(&c->client_version)) {
        if (c->client.version_requested) {
            uo_client_send(c->client.client, c->client_version.packet,
                           c->client_version.packet_length);
            c->client.version_requested = false;
        }

        return PA_DROP;
    } else {
        const bool was_unkown = c->client_version.protocol == PROTOCOL_UNKNOWN;

        int ret = client_version_copy(&c->client_version, p, length);
        if (ret > 0) {
            if (was_unkown && c->client.client != nullptr)
                uo_client_set_protocol(c->client.client,
                                       c->client_version.protocol);
            LogFormat(2, "emulating client version '%s', protocol '%s'\n",
                      c->client_version.packet->version,
                      protocol_name(c->client_version.protocol));
        } else if (ret == 0)
            LogFormat(2, "invalid client version\n");
        return PA_ACCEPT;
    }
}

static packet_action_t
handle_extended(LinkedServer *ls __attr_unused,
                const void *data, size_t length)
{
    auto p = (const struct uo_packet_extended *)data;

    if (length < sizeof(*p))
        return PA_DISCONNECT;

    LogFormat(8, "from client: extended 0x%04x\n", ntohs(p->extended_cmd));

    return PA_ACCEPT;
}

static packet_action_t
handle_hardware(LinkedServer *ls,
                const void *data __attr_unused, size_t length __attr_unused)
{
    if (ls->connection->instance->config->antispy)
        return PA_DROP;

    return PA_ACCEPT;
}

static packet_action_t
handle_seed(LinkedServer *ls,
            const void *data, size_t length __attr_unused)
{
    auto p = (const struct uo_packet_seed *)data;

    assert(length == sizeof(*p));

    if (ls->client_version.seed == nullptr) {
        client_version_seed(&ls->client_version, p);
        uo_server_set_protocol(ls->server, ls->client_version.protocol);

        LogFormat(2, "detected client 6.0.5.0 or newer (%u.%u.%u.%u)\n",
                  (unsigned)ntohl(p->client_major),
                  (unsigned)ntohl(p->client_minor),
                  (unsigned)ntohl(p->client_revision),
                  (unsigned)ntohl(p->client_patch));
    }

    if (!client_version_defined(&ls->connection->client_version) &&
        ls->connection->client_version.seed == nullptr) {
        client_version_seed(&ls->connection->client_version, p);

        if (ls->connection->client.client != nullptr)
            uo_client_set_protocol(ls->connection->client.client,
                                   ls->connection->client_version.protocol);
    }

    return PA_DROP;
}

struct server_packet_binding client_packet_bindings[] = {
    { .cmd = PCK_CreateCharacter,
      .handler = handle_create_character,
    },
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
    { .cmd = PCK_Drop, /* 0x08 */
      .handler = handle_drop,
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
    { .cmd = PCK_Seed, /* 0xef */
      .handler = handle_seed,
    },
    { .handler = nullptr }
};

