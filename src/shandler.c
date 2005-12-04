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
#include <errno.h>
#include <string.h>

#include "packets.h"
#include "handler.h"
#include "relay.h"
#include "connection.h"
#include "server.h"

static packet_action_t handle_ping(struct connection *c,
                                   void *data, size_t length) {
    struct uo_packet_ping *p = data;

    assert(length == sizeof(*p));

    c->ping_ack = p->id;

    return PA_DROP;
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

    count = ntohs(*(uint16_t*)(p + 4));
    printf("serverlist: %u servers\n", count);
    if (length != 6 + count * sizeof(*server_info))
        return PA_DISCONNECT;

    server_info = (struct uo_fragment_server_info*)(p + 6);
    for (i = 0; i < count; i++, server_info++) {
        k = ntohs(server_info->index);
        if (k != i)
            return PA_DISCONNECT;

        printf("server %u: name=%s address=0x%08x\n",
               ntohs(server_info->index),
               server_info->name,
               ntohl(server_info->address));
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

    assert(length == sizeof(*p));

    if (c->server == NULL)
        return PA_ACCEPT;

    printf("relay: address=0x%08x port=%u\n",
           ntohl(p->ip), ntohs(p->port));

    /* remember the original IP/port */
    relay = (struct relay){
        .auth_id = p->auth_id,
        .server_ip = p->ip,
        .server_port = p->port,
    };

    relay_add(c->instance->relays, &relay);

    /* get our local address */
    ret = getsockname(uo_server_fileno(c->server),
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

    printf("new relay: address=0x%08x port=%u\n",
           ntohl(p->ip), ntohs(p->port));

    return PA_ACCEPT;
}

static packet_action_t handle_supported_features(struct connection *c,
                                                 void *data, size_t length) {
    struct uo_packet_supported_features *p = data;

    assert(length == sizeof(*p));

    c->supported_features_flags = p->flags;

    return PA_ACCEPT;
}

static packet_action_t handle_start(struct connection *c,
                                    void *data, size_t length) {
    struct uo_packet_start *p = data;

    assert(length == sizeof(*p));

    c->packet_start = *p;

    return PA_ACCEPT;
}

static packet_action_t handle_zone_change(struct connection *c,
                                          void *data, size_t length) {
    struct uo_packet_zone_change *p = data;

    assert(length == sizeof(*p));

    c->packet_start.x = p->x;
    c->packet_start.y = p->y;
    c->packet_start.z = p->z;
    c->packet_start.map_width = p->map_width;
    c->packet_start.map_height = p->map_height;

    return PA_ACCEPT;
}

static packet_action_t handle_mobile_update(struct connection *c,
                                            void *data, size_t length) {
    struct uo_packet_mobile_update *p = data;

    fprintf(stderr, "l1=%lu l2=%lu\n",
            length, sizeof(*p));
    assert(length == sizeof(*p));

    if (c->packet_start.serial == p->serial) {
        c->packet_start.body = p->body;
        c->packet_start.x = p->x;
        c->packet_start.y = p->y;
        c->packet_start.z = p->z;
        c->packet_start.direction = p->direction;
    }

    return PA_ACCEPT;
}

struct packet_binding server_packet_bindings[] = {
    { .cmd = PCK_Ping,
      .handler = handle_ping,
    },
    { .cmd = PCK_ServerList,
      .handler = handle_server_list,
    },
    { .cmd = PCK_Relay,
      .handler = handle_relay,
    },
    { .cmd = PCK_SupportedFeatures,
      .handler = handle_supported_features,
    },
    { .cmd = PCK_Start,
      .handler = handle_start,
    },
    { .cmd = PCK_ZoneChange,
      .handler = handle_zone_change,
    },
    { .cmd = PCK_MobileUpdate,
      .handler = handle_mobile_update,
    },
    { .handler = NULL }
};
