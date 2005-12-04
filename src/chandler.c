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
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>

#include "packets.h"
#include "handler.h"
#include "connection.h"
#include "client.h"
#include "server.h"
#include "relay.h"

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

        uo_server_send(c->server, &p2, sizeof(p2));

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

        uo_server_send(c->server, &p2, sizeof(p2));

        return PA_DROP;
    }

    return PA_ACCEPT;
}

static packet_action_t handle_ping(struct connection *c,
                                   void *data, size_t length) {
    uo_server_send(c->server, data, length);
    return PA_DROP;
}

static packet_action_t handle_account_login(struct connection *c,
                                            void *data, size_t length) {
    const struct uo_packet_account_login *p = data;
    int ret;

    assert(length == sizeof(*p));
    assert(sizeof(p->username) == sizeof(c->username));
    assert(sizeof(p->password) == sizeof(c->password));

#ifdef DUMP_LOGIN
    printf("account_login: username=%s password=%s\n",
           p->username, p->password);
#endif

    if (c->client != NULL) {
        fprintf(stderr, "already logged in\n");
        return PA_DISCONNECT;
    }

    ret = uo_client_create(c->instance->login_address,
                           uo_server_seed(c->server),
                           &c->client);
    if (ret != 0) {
        struct uo_packet_login_bad response;

        fprintf(stderr, "uo_client_create() failed: %s\n",
                strerror(-ret));

        response.cmd = PCK_LogBad;
        response.reason = 0x02; /* blocked */

        uo_server_send(c->server, &response,
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
    struct addrinfo server_address;
    struct sockaddr_in sin;

    assert(length == sizeof(*p));
    assert(sizeof(p->username) == sizeof(c->username));
    assert(sizeof(p->password) == sizeof(c->password));

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
            memcmp(p->password, c2->password, sizeof(c2->password)) == 0) {
            uo_server_shift(c->server, length); /* XXX */

            if (c2->server != NULL) {
                /* disconnect old client */
                uo_server_dispose(c2->server);
                c2->server = NULL;
            }

#ifdef DUMP_LOGIN
            printf("attaching connection\n");
#endif
            attach_after_game_login(c2, c);

            return PA_DISCONNECT;
        }
    }

    sin.sin_family = AF_INET;
    sin.sin_port = relay->server_port;
    sin.sin_addr.s_addr = relay->server_ip;

    memset(&server_address, 0, sizeof(server_address));
    server_address.ai_family = AF_INET;
    server_address.ai_addrlen = sizeof(sin);
    server_address.ai_addr = (struct sockaddr*)&sin;

    ret = uo_client_create(&server_address,
                           uo_server_seed(c->server),
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

    if (c->attaching) {
        printf("attaching connection, stage II\n");
        attach_after_play_character(c);
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
    { .cmd = PCK_ExtData,
      .handler = handle_extended,
    },
    { .handler = NULL }
};

