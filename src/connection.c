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

#include <assert.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>

#include "connection.h"
#include "server.h"
#include "client.h"
#include "relay.h"
#include "handler.h"
#include "packets.h"

int connection_new(struct instance *instance,
                   int server_socket,
                   struct connection **connectionp) {
    struct connection *c;
    int ret;

    c = calloc(1, sizeof(*c));
    if (c == NULL)
        return -ENOMEM;

    c->instance = instance;

    ret = uo_server_create(server_socket, &c->server);
    if (ret != 0) {
        connection_delete(c);
        return ret;
    }

    *connectionp = c;

    return 0;
}

void connection_delete(struct connection *c) {
    while (c->items_head != NULL) {
        struct item *i = c->items_head;
        c->items_head = i->next;

        free(i);
    }

    while (c->mobiles_head != NULL) {
        struct mobile *m = c->mobiles_head;
        c->mobiles_head = m->next;

        if (m->packet_mobile_incoming != NULL)
            free(m->packet_mobile_incoming);
        if (m->packet_mobile_status != NULL)
            free(m->packet_mobile_status);
        free(m);
    }

    if (c->server != NULL)
        uo_server_dispose(c->server);
    if (c->client != NULL)
        uo_client_dispose(c->client);

    free(c);
}

void connection_invalidate(struct connection *c) {
    c->invalid = 1;

    if (c->server != NULL) {
        uo_server_dispose(c->server);
        c->server = NULL;
    }

    if (c->client != NULL) {
        uo_client_dispose(c->client);
        c->client = NULL;
    }
}

void connection_pre_select(struct connection *c, struct selectx *sx) {
    if (c->invalid)
        return;

    if (c->client != NULL &&
        !uo_client_alive(c->client)) {
        printf("server disconnected\n");
        connection_invalidate(c);
        return;
    }

    if (c->server != NULL &&
        !uo_server_alive(c->server)) {
        fprintf(stderr, "client disconnected\n");
        connection_invalidate(c);
    }

    if (c->client != NULL)
        uo_client_pre_select(c->client, sx);

    if (c->server != NULL)
        uo_server_pre_select(c->server, sx);
}

int connection_post_select(struct connection *c, struct selectx *sx) {
    void *p;
    size_t length;
    packet_action_t action;

    if (c->invalid)
        return 0;

    if (c->client != NULL) {
        uo_client_post_select(c->client, sx);
        while (c->client != NULL &&
               (p = uo_client_peek(c->client, &length)) != NULL) {
            action = handle_packet(server_packet_bindings,
                                   c, p, length);
            switch (action) {
            case PA_ACCEPT:
                if (c->server != NULL)
                    uo_server_send(c->server, p, length);
                break;

            case PA_DROP:
                break;

            case PA_DISCONNECT:
                connection_invalidate(c);
                break;
            }

            if (c->client != NULL)
                uo_client_shift(c->client, length);
        }
    }

    if (c->server != NULL) {
        uo_server_post_select(c->server, sx);
        while (c->server != NULL &&
               (p = uo_server_peek(c->server, &length)) != NULL) {
            action = handle_packet(client_packet_bindings,
                                   c, p, length);
            switch (action) {
            case PA_ACCEPT:
                if (c->client != NULL)
                    uo_client_send(c->client, p, length);
                break;

            case PA_DROP:
                break;

            case PA_DISCONNECT:
                connection_invalidate(c);
                break;
            }

            if (c->server != NULL)
                uo_server_shift(c->server, length);
        }
    }

    return 0;
}

void connection_idle(struct connection *c) {
    if (c->invalid)
        return;

    if (c->client != NULL) {
        struct uo_packet_ping ping;

        ping.cmd = PCK_Ping;
        ping.id = ++c->ping_request;

        fprintf(stderr, "sending ping\n");
        uo_client_send(c->client, &ping, sizeof(ping));
    }
}

static struct item **find_item(struct connection *c,
                               u_int32_t serial) {
    struct item **i = &c->items_head;

    while (*i != NULL) {
        if ((*i)->serial == serial)
            return i;
        i = &(*i)->next;
    }

    return i;
}

void connection_add_item(struct connection *c,
                         const struct uo_packet_put *p) {
    u_int32_t serial = p->serial & 0x7fffffff;
    struct item **ip, *i;

    assert(p->cmd == PCK_Put);
    assert(ntohs(p->length) <= sizeof(*p));

    ip = find_item(c, serial);
    if (*ip == NULL) {
        i = calloc(1, sizeof(*i));
        if (i == NULL) {
            fprintf(stderr, "out of memory\n");
            return;
        }

        *ip = i;
        i->serial = serial;
    } else {
        i = *ip;
    }

    i->packet_put = *p;
}

static struct mobile **find_mobile(struct connection *c,
                                   u_int32_t serial) {
    struct mobile **m = &c->mobiles_head;

    while (*m != NULL) {
        if ((*m)->serial == serial)
            return m;
        m = &(*m)->next;
    }

    return m;
}

static struct mobile *add_mobile(struct connection *c,
                                 u_int32_t serial) {
    struct mobile **mp, *m;

    mp = find_mobile(c, serial);
    if (*mp != NULL)
        return *mp;

    *mp = m = calloc(1, sizeof(*m));
    if (m == NULL) {
        fprintf(stderr, "out of memory\n");
        return NULL;
    }

    m->serial = serial;

    return m;
}

static void replace_packet(void **destp, const void *src,
                           size_t length) {
    assert(length == get_packet_length(src, length));

    if (*destp != NULL)
        free(*destp);

    *destp = malloc(length);
    if (*destp == NULL) {
        fprintf(stderr, "out of memory\n");
        return;
    }

    memcpy(*destp, src, length);
}

void connection_mobile_incoming(struct connection *c,
                                const struct uo_packet_mobile_incoming *p) {
    struct mobile *m;

    assert(p->cmd == PCK_MobileIncoming);

    m = add_mobile(c, p->serial);
    if (m == NULL)
        return;

    replace_packet((void**)&m->packet_mobile_incoming,
                   p, ntohs(p->length));
}

void connection_mobile_status(struct connection *c,
                              const struct uo_packet_mobile_status *p) {
    struct mobile *m;

    assert(p->cmd == PCK_MobileStatus);

    m = add_mobile(c, p->serial);
    if (m == NULL)
        return;

    /* XXX: check if p->flags is available */
    if (m->packet_mobile_status == NULL ||
        m->packet_mobile_status->flags < p->flags)
        replace_packet((void**)&m->packet_mobile_status,
                       p, ntohs(p->length));
}
