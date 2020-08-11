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
#include "PacketStructs.hxx"
#include "Handler.hxx"
#include "CVersion.hxx"
#include "Connection.hxx"
#include "LinkedServer.hxx"
#include "Client.hxx"
#include "Server.hxx"
#include "Config.hxx"
#include "Log.hxx"
#include "Bridge.hxx"
#include "util/Compiler.h"
#include "util/VarStructPtr.hxx"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
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

static char *simple_unicode_to_ascii(char *dest, const PackedBE16 *src,
                                     size_t length) {
    size_t position;

    for (position = 0; position < length && src[position] != 0; position++) {
        uint16_t ch = src[position];
        if (ch & 0xff00)
            return nullptr;

        dest[position] = (char)ch;
    }

    dest[position] = 0;

    return dest;
}

static PacketAction
handle_talk(LinkedServer *ls,
            const char *text)
{
    /* the percent sign introduces an uoproxy command */
    if (text[0] == '%') {
        connection_handle_command(ls, text + 1);
        return PacketAction::DROP;
    }

    return PacketAction::ACCEPT;
}

static PacketAction
handle_create_character(LinkedServer *ls,
                        const void *data, [[maybe_unused]] size_t length)
{
    auto p = (const struct uo_packet_create_character *)data;
    assert(length == sizeof(*p));

    if (ls->connection->instance.config.antispy) {
        struct uo_packet_create_character q = *p;
        q.client_ip = 0xc0a80102;
        uo_client_send(ls->connection->client.client, &q, sizeof(q));
        return PacketAction::DROP;
    } else
        return PacketAction::ACCEPT;
}

static PacketAction
handle_walk(LinkedServer *ls,
            const void *data, [[maybe_unused]] size_t length)
{
    auto p = (const struct uo_packet_walk *)data;

    assert(length == sizeof(*p));

    if (!ls->connection->IsInGame())
        return PacketAction::DISCONNECT;

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

        return PacketAction::DROP;
    }

    connection_walk_request(ls, p);

    return PacketAction::DROP;
}

static PacketAction
handle_talk_ascii(LinkedServer *ls,
                  const void *data, size_t length)
{
    auto p = (const struct uo_packet_talk_ascii *)data;

    if (length < sizeof(*p))
        return PacketAction::DISCONNECT;

    const size_t text_length = length - sizeof(*p);
    if (p->text[text_length] != 0)
        return PacketAction::DISCONNECT;

    return handle_talk(ls, p->text);
}

static PacketAction
handle_use(LinkedServer *ls,
           const void *data, [[maybe_unused]] size_t length)
{
    [[maybe_unused]] auto p = (const struct uo_packet_use *)data;

    assert(length == sizeof(*p));

    if (ls->connection->client.reconnecting) {
        uo_server_speak_console(ls->server,
                                "please wait until uoproxy finishes reconnecting");
        return PacketAction::DROP;
    }

#ifdef DUMP_USE
    do {
        Item *i = connection_find_item(c, p->serial);
        if (i == nullptr) {
            LogFormat(7, "Use 0x%x\n", (unsigned)p->serial);
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
                      (unsigned)p->serial, (unsigned)item_id);
        }
        fflush(stdout);
    } while (0);
#endif

    return PacketAction::ACCEPT;
}

static PacketAction
handle_action(LinkedServer *ls, const void *, size_t)
{
    if (ls->connection->client.reconnecting) {
        uo_server_speak_console(ls->server,
                                "please wait until uoproxy finishes reconnecting");
        return PacketAction::DROP;
    }

    return PacketAction::ACCEPT;
}

static PacketAction
handle_lift_request(LinkedServer *ls,
                    const void *data, [[maybe_unused]] size_t length)
{
    [[maybe_unused]] auto p = (const struct uo_packet_lift_request *)data;

    assert(length == sizeof(*p));

    if (ls->connection->client.reconnecting) {
        /* while reconnecting, reject all lift requests */
        struct uo_packet_lift_reject p2 = {
            .cmd = PCK_LiftReject,
            .reason = 0x00, /* CannotLift */
        };

        uo_server_send(ls->server, &p2, sizeof(p2));

        return PacketAction::DROP;
    }

    return PacketAction::ACCEPT;
}

static PacketAction
handle_drop(LinkedServer *ls,
            const void *data, [[maybe_unused]] size_t length)
{
    auto *client = &ls->connection->client;

    if (!client->IsInGame() || client->reconnecting ||
        client->client == nullptr)
        return PacketAction::DROP;

    if (ls->client_version.protocol < PROTOCOL_6) {
        auto p = (const struct uo_packet_drop *)data;

        assert(length == sizeof(*p));

        if (ls->connection->client_version.protocol < PROTOCOL_6)
            return PacketAction::ACCEPT;

        struct uo_packet_drop_6 p6;
        drop_5_to_6(&p6, p);
        uo_client_send(client->client, &p6, sizeof(p6));
    } else {
        auto p = (const struct uo_packet_drop_6 *)data;

        assert(length == sizeof(*p));

        if (ls->connection->client_version.protocol >= PROTOCOL_6)
            return PacketAction::ACCEPT;

        struct uo_packet_drop p5;
        drop_6_to_5(&p5, p);
        uo_client_send(client->client, &p5, sizeof(p5));
    }

    return PacketAction::DROP;
}

static PacketAction
handle_resynchronize(LinkedServer *ls, const void *, size_t)
{
    LogFormat(3, "Resync!\n");

    ls->connection->walk.seq_next = 0;

    return PacketAction::ACCEPT;
}

static PacketAction
handle_target(LinkedServer *ls,
              const void *data, [[maybe_unused]] size_t length)
{
    [[maybe_unused]] auto p = (const struct uo_packet_target *)data;
    World *world = &ls->connection->client.world;

    assert(length == sizeof(*p));

    if (world->packet_target.cmd == PCK_Target &&
        world->packet_target.target_id != 0) {
        /* cancel this target for all other clients */
        memset(&world->packet_target, 0,
               sizeof(world->packet_target));
        world->packet_target.cmd = PCK_Target;
        world->packet_target.flags = 3;

        ls->connection->BroadcastToInGameClientsExcept(&world->packet_target,
                                                       sizeof(world->packet_target),
                                                       *ls);
    }

    return PacketAction::ACCEPT;
}

static PacketAction
handle_ping(LinkedServer *ls,
            const void *data, size_t length)
{
    uo_server_send(ls->server, data, length);
    return PacketAction::DROP;
}

static PacketAction
handle_account_login(LinkedServer *ls,
                     const void *data, size_t length)
{
    auto p = (const struct uo_packet_account_login *)data;
    Connection *c = ls->connection;
    const Config &config = c->instance.config;

    assert(length == sizeof(*p));

    if (c->IsInGame())
        return PacketAction::DISCONNECT;

#ifdef DUMP_LOGIN
    LogFormat(7, "account_login: username=%s password=%s\n",
              p->username, p->password);
#endif

    if (c->client.client != nullptr) {
        LogFormat(2, "already logged in\n");
        return PacketAction::DISCONNECT;
    }

    c->credentials = p->credentials;

    Connection *other = c->instance.FindAttachConnection(c->credentials);
    assert(other != c);
    if (other != nullptr) {
        /* attaching to an existing connection, fake the server
           list */
        struct uo_packet_server_list p2;
        memset(&p2, 0, sizeof(p2));

        p2.cmd = PCK_ServerList;
        p2.length = sizeof(p2);
        p2.unknown_0x5d = 0x5d;
        p2.num_game_servers = 1;

        p2.game_servers[0].index = 0;
        strcpy(p2.game_servers[0].name, "attach");
        p2.game_servers[0].address = 0xdeadbeef;

        uo_server_send(ls->server, &p2, sizeof(p2));
        return PacketAction::DROP;
    }

    if (config.num_game_servers > 0) {
        /* we have a game server list and thus we emulate the login
           server */
        unsigned i, num_game_servers = config.num_game_servers;
        struct game_server_config *game_servers = config.game_servers;
        struct sockaddr_in *sin;

        assert(config.game_servers != nullptr);

        struct uo_packet_server_list *p2;
        length = sizeof(*p2) + (num_game_servers - 1) * sizeof(p2->game_servers[0]);

        const VarStructPtr<struct uo_packet_server_list> p2_(length);
        p2 = p2_.get();

        p2->cmd = PCK_ServerList;
        p2->length = length;
        p2->unknown_0x5d = 0x5d;
        p2->num_game_servers = num_game_servers;

        for (i = 0; i < num_game_servers; i++) {
            p2->game_servers[i].index = i;
            snprintf(p2->game_servers[i].name, sizeof(p2->game_servers[i].name),
                     "%s", game_servers[i].name);

            if (game_servers[i].address->ai_family != AF_INET)
                continue;

            sin = (struct sockaddr_in*)game_servers[i].address->ai_addr;
            p2->game_servers[i].address = sin->sin_addr.s_addr;
        }

        uo_server_send(ls->server, p2_.get(), p2_.size());
        return PacketAction::DROP;
    } else if (config.login_address != nullptr) {
        /* connect to the real login server */
        uint32_t seed;

        if (config.antispy)
            /* since the login server seed usually contains the
               internal IP address of the client, we want to hide it
               in antispy mode - always send 192.168.1.2 which is
               generic enough to be useless */
            seed = 0xc0a80102;
        else
            seed = uo_server_seed(ls->server);

        int ret = c->Connect(config.login_address->ai_addr,
                             config.login_address->ai_addrlen,
                             seed);
        if (ret != 0) {
            struct uo_packet_account_login_reject response;

            log_error("connection to login server failed", ret);

            response.cmd = PCK_AccountLoginReject;
            response.reason = 0x02; /* blocked */

            uo_server_send(ls->server, &response,
                           sizeof(response));
            return PacketAction::DROP;
        }

        return PacketAction::ACCEPT;
    } else {
        /* should not happen */

        return PacketAction::DISCONNECT;
    }
}

static PacketAction
handle_game_login(LinkedServer *ls,
                  const void *data, [[maybe_unused]] size_t length)
{
    auto p = (const struct uo_packet_game_login *)data;

    assert(length == sizeof(*p));

    if (ls->connection->instance.config.razor_workaround) {
        bool was_attach = false;

        /* I have observed the Razor client ignoring the redirect if the IP
           address differs from what it connected to.  (I guess this is a bug in
           RunUO & Razor).  In that case it does a gamelogin on the old
           linked_server without reconnecting to us.

           So we apply the zombie-lookup only if the remote UO client actually
           did bother to reconnet to us. */
        if (!ls->connection->client.IsConnected()) {
            auto &obsolete_connection = *ls->connection;
            auto &instance = obsolete_connection.instance;

            /* this should only happen in redirect mode.. so look for the
               correct zombie so that we can re-use its connection to the UO
               server. */
            LinkedServer *zombie = instance.FindZombie(*p);
            if (zombie == nullptr) {
                /* houston, we have a problem -- reject the game login -- it
                   either came in too slowly (and so we already reaped the
                   zombie) or it was a hack attempt (wrong password) */
                LogFormat(2, "could not find previous connection for redirected client"
                          " -- disconnecting client!\n");
                return PacketAction::DISCONNECT;
            }

            auto &existing_connection = *zombie->connection;

            /* found it! Eureka! */
            zombie->expecting_reconnect = false;
            was_attach = zombie->attaching;
            zombie->attaching = false;


            /* copy the previously detected protocol version */
            if (!was_attach)
                existing_connection.client_version.protocol = obsolete_connection.client_version.protocol;

            /* remove the object from the old connection */
            obsolete_connection.Remove(*ls);
            obsolete_connection.Destroy();

            LogFormat(2, "attaching redirected client to its previous connection\n");

            existing_connection.Add(*ls);

            /* delete the zombie, we don't need it anymore */
            existing_connection.Remove(*zombie);
            delete zombie;
        } else
            was_attach = ls->attaching;
        /* after GameLogin, must enable compression. */
        uo_server_set_compression(ls->server, true);
        ls->got_gamelogin = true;
        ls->attaching = false;
        if (ls->connection->IsInGame() && was_attach) {
            /* already in game .. this was likely an attach connection */
            attach_send_world(ls);
        } else if (ls->connection->client.char_list) {
            uo_server_send(ls->server,
                           ls->connection->client.char_list.get(),
                           ls->connection->client.char_list.size());
        }
        ls->expecting_reconnect = false;
        return PacketAction::DROP;
    }

    /* Unless we're in razor workaround mode, valid UO clients will never send
       this packet since we're hiding redirects from them. */
    return PacketAction::DISCONNECT;
}

static PacketAction
handle_play_character(LinkedServer *ls,
                      const void *data, [[maybe_unused]] size_t length)
{
    auto p = (const struct uo_packet_play_character *)data;

    assert(length == sizeof(*p));

    ls->connection->character_index = p->slot;

    return PacketAction::ACCEPT;
}

static void
redirect_to_self(LinkedServer *ls)
{
    struct uo_packet_relay relay;
    static uint32_t authid = 0;
    struct in_addr addr;

    if (!authid) authid = time(0);

    relay.cmd = PCK_Relay;
    relay.port = PackedBE16::FromBE(uo_server_getsockport(ls->server));
    relay.ip = PackedBE32::FromBE(uo_server_getsockname(ls->server));
    addr.s_addr = relay.ip.raw();
    LogFormat(8, "redirecting to: %s:%u\n",
              inet_ntoa(addr), (unsigned)relay.port);;
    relay.auth_id = ls->auth_id = authid++;
    ls->expecting_reconnect = true;
    uo_server_send(ls->server, &relay, sizeof(relay));
}

static PacketAction
handle_play_server(LinkedServer *ls,
                   const void *data, [[maybe_unused]] size_t length)
{
    auto p = (const struct uo_packet_play_server *)data;
    Connection *c = ls->connection, *c2;
    auto &instance = c->instance;
    const auto &config = instance.config;
    PacketAction retaction = PacketAction::DROP;

    assert(length == sizeof(*p));

    if (c->IsInGame())
        return PacketAction::DISCONNECT;

    assert(std::next(c->servers.iterator_to(*ls)) == c->servers.end());

    c->server_index = p->index;

    c2 = instance.FindAttachConnection(*c);
    if (c2 != nullptr) {
        /* remove the object from the old connection */
        c->Remove(*ls);
        c->Destroy();

        c2->Add(*ls);

        if (config.razor_workaround) { ///< need to send redirect
            /* attach it to the new connection and send redirect (below) */
            ls->attaching = true;
        }  else {
            /* attach it to the new connection and begin playing right away */
            LogFormat(2, "attaching connection\n");
            attach_send_world(ls);
        }

        retaction = PacketAction::DROP;
    } else if (config.login_address == nullptr &&
               config.game_servers != nullptr &&
               config.num_game_servers > 0) {
        const unsigned num_game_servers = config.num_game_servers;
        int ret;
        struct uo_packet_game_login login;
        uint32_t seed;

        assert(c->client.client == nullptr);

        /* locate the selected game server */
        unsigned i = p->index;
        if (i >= num_game_servers)
            return PacketAction::DISCONNECT;

        const auto &server_config = config.game_servers[i];

        /* connect to new server */

        if (c->client_version.seed != nullptr)
            seed = c->client_version.seed->seed;
        else
            seed = 0xc0a80102; /* 192.168.1.2 */

        ret = c->Connect(server_config.address->ai_addr,
                         server_config.address->ai_addrlen, seed);
        if (ret != 0) {
            log_error("connect to game server failed", ret);
            return PacketAction::DISCONNECT;
        }

        /* send game login to new server */
        login.cmd = PCK_GameLogin;
        login.auth_id = seed;
        login.credentials = c->credentials;

        uo_client_send(c->client.client, &login, sizeof(login));

        retaction = PacketAction::DROP;
    } else
        retaction = PacketAction::ACCEPT;

    if (config.razor_workaround) {
        /* Razor workaround -> send the redirect packet to the client and tell
           them to redirect to self!  This is because Razor refuses to work if
           it doesn't see a redirect packet.  Note that after the redirect,
           the client immediately sends 'GameLogin' which means we turn
           compression on. */
        redirect_to_self(ls);
    }

    return retaction;
}

static PacketAction
handle_spy(LinkedServer *ls,
           const void *data, size_t length)
{
    (void)data;
    (void)length;

    if (ls->connection->instance.config.antispy)
        return PacketAction::DROP;

    return PacketAction::ACCEPT;
}

static PacketAction
handle_talk_unicode(LinkedServer *ls,
                    const void *data, size_t length)
{
    auto p = (const struct uo_packet_talk_unicode *)data;

    if (length < sizeof(*p))
        return PacketAction::DISCONNECT;

    if (p->type == 0xc0) {
        uint16_t value = p->text[0];
        unsigned num_keywords = (value & 0xfff0) >> 4;
        unsigned skip_bits = (num_keywords + 1) * 12;
        unsigned skip_bytes = 12 + (skip_bits + 7) / 8;
        const char *start = (const char *)data;
        const char *t = start + skip_bytes;
        size_t text_length = length - skip_bytes - 1;

        if (skip_bytes >= length)
            return PacketAction::DISCONNECT;

        if (t[0] == 0 || t[text_length] != 0)
            return PacketAction::DISCONNECT;

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

    return PacketAction::ACCEPT;
}

static PacketAction
handle_gump_response(LinkedServer *ls,
                     const void *data, size_t length)
{
    auto p = (const struct uo_packet_gump_response *)data;

    if (length < sizeof(*p))
        return PacketAction::DISCONNECT;

    /* close the gump on all other clients */
    const struct uo_packet_close_gump close = {
        .cmd = PCK_Extended,
        .length = sizeof(close),
        .extended_cmd = 0x0004,
        .type_id = p->type_id,
        .button_id = 0,
    };

    ls->connection->BroadcastToInGameClientsExcept(&close, sizeof(close), *ls);

    return PacketAction::ACCEPT;
}

static PacketAction
handle_client_version(LinkedServer *ls,
                      const void *data, size_t length)
{
    Connection *c = ls->connection;
    auto p = (const struct uo_packet_client_version *)data;

    if (!ls->client_version.IsDefined()) {
        bool was_unkown = ls->client_version.protocol == PROTOCOL_UNKNOWN;
        int ret = ls->client_version.Set(p, length);
        if (ret > 0) {
            if (was_unkown)
                uo_server_set_protocol(ls->server,
                                       ls->client_version.protocol);

            LogFormat(2, "client version '%s', protocol '%s'\n",
                      ls->client_version.packet->version,
                      protocol_name(ls->client_version.protocol));
        }
    }

    if (c->client_version.IsDefined()) {
        if (c->client.version_requested) {
            uo_client_send(c->client.client, c->client_version.packet.get(),
                           c->client_version.packet.size());
            c->client.version_requested = false;
        }

        return PacketAction::DROP;
    } else {
        const bool was_unkown = c->client_version.protocol == PROTOCOL_UNKNOWN;

        int ret = c->client_version.Set(p, length);
        if (ret > 0) {
            if (was_unkown && c->client.client != nullptr)
                uo_client_set_protocol(c->client.client,
                                       c->client_version.protocol);
            LogFormat(2, "emulating client version '%s', protocol '%s'\n",
                      c->client_version.packet->version,
                      protocol_name(c->client_version.protocol));
        } else if (ret == 0)
            LogFormat(2, "invalid client version\n");
        return PacketAction::ACCEPT;
    }
}

static PacketAction
handle_extended(LinkedServer *, const void *data, size_t length)
{
    auto p = (const struct uo_packet_extended *)data;

    if (length < sizeof(*p))
        return PacketAction::DISCONNECT;

    LogFormat(8, "from client: extended 0x%04x\n", (unsigned)p->extended_cmd);

    return PacketAction::ACCEPT;
}

static PacketAction
handle_hardware(LinkedServer *ls, const void *, size_t)
{
    if (ls->connection->instance.config.antispy)
        return PacketAction::DROP;

    return PacketAction::ACCEPT;
}

static PacketAction
handle_seed(LinkedServer *ls, const void *data, [[maybe_unused]] size_t length)
{
    auto p = (const struct uo_packet_seed *)data;

    assert(length == sizeof(*p));

    if (ls->client_version.seed == nullptr) {
        ls->client_version.Seed(*p);
        uo_server_set_protocol(ls->server, ls->client_version.protocol);

        LogFormat(2, "detected client 6.0.5.0 or newer (%u.%u.%u.%u)\n",
                  (unsigned)p->client_major,
                  (unsigned)p->client_minor,
                  (unsigned)p->client_revision,
                  (unsigned)p->client_patch);
    }

    if (!ls->connection->client_version.IsDefined() &&
        ls->connection->client_version.seed == nullptr) {
        ls->connection->client_version.Seed(*p);

        if (ls->connection->client.client != nullptr)
            uo_client_set_protocol(ls->connection->client.client,
                                   ls->connection->client_version.protocol);
    }

    return PacketAction::DROP;
}

const struct server_packet_binding client_packet_bindings[] = {
    { PCK_CreateCharacter, handle_create_character },
    { PCK_Walk, handle_walk },
    { PCK_TalkAscii, handle_talk_ascii },
    { PCK_Use, handle_use },
    { PCK_Action, handle_action },
    { PCK_LiftRequest, handle_lift_request },
    { PCK_Drop, handle_drop }, /* 0x08 */
    { PCK_Resynchronize, handle_resynchronize },
    { PCK_Target, handle_target }, /* 0x6c */
    { PCK_Ping, handle_ping },
    { PCK_AccountLogin, handle_account_login },
    { PCK_AccountLogin2, handle_account_login },
    { PCK_GameLogin, handle_game_login },
    { PCK_PlayCharacter, handle_play_character },
    { PCK_PlayServer, handle_play_server },
    { PCK_Spy, handle_spy }, /* 0xa4 */
    { PCK_TalkUnicode, handle_talk_unicode },
    { PCK_GumpResponse, handle_gump_response },
    { PCK_ClientVersion, handle_client_version }, /* 0xbd */
    { PCK_Extended, handle_extended },
    { PCK_Hardware, handle_hardware }, /* 0xd9 */
    { PCK_Seed, handle_seed }, /* 0xef */
    {}
};
