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
#include <string.h>

#include "connection.h"

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

void connection_remove_item(struct connection *c, u_int32_t serial) {
    struct item **ip, *i;

    ip = find_item(c, serial);
    i = *ip;
    if (i == NULL)
        return;

    *ip = i->next;

    free(i);
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

void connection_remove_mobile(struct connection *c, u_int32_t serial) {
    struct mobile **mp, *m;

    mp = find_mobile(c, serial);
    m = *mp;
    if (m == NULL)
        return;

    *mp = m->next;

    free(m);
}
