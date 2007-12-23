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

#include <assert.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "connection.h"
#include "server.h"

struct item *
world_find_item(struct world *world,
                u_int32_t serial)
{
    struct item *item;

    list_for_each_entry(item, &world->items, siblings) {
        if (item->serial == serial)
            return item;
    }

    return NULL;
}

static struct item *
make_item(struct world *world, u_int32_t serial)
{
    struct item *i;

    i = world_find_item(world, serial);
    if (i != NULL)
        return i;

    i = calloc(1, sizeof(*i));
    if (i == NULL)
        return NULL;

    i->serial = serial;

    list_add(&i->siblings, &world->items);

    return i;
}

void
world_world_item(struct world *world,
                 const struct uo_packet_world_item *p)
{
    struct item *i;

    assert(p->cmd == PCK_WorldItem);
    assert(ntohs(p->length) <= sizeof(*p));

    i = make_item(world, p->serial & htonl(0x7fffffff));
    if (i == NULL) {
        fprintf(stderr, "out of memory\n");
        return;
    }

    i->packet_world_item = *p;
}

void
world_equip(struct world *world,
            const struct uo_packet_equip *p)
{
    struct item *i;

    assert(p->cmd == PCK_Equip);

    i = make_item(world, p->serial);
    if (i == NULL) {
        fprintf(stderr, "out of memory\n");
        return;
    }

    i->packet_equip = *p;
}

void
world_container_open(struct world *world,
                     const struct uo_packet_container_open *p)
{
    struct item *i;

    assert(p->cmd == PCK_ContainerOpen);

    i = make_item(world, p->serial);
    if (i == NULL) {
        fprintf(stderr, "out of memory\n");
        return;
    }

    i->packet_container_open = *p;
}

void
world_container_update(struct world *world,
                       const struct uo_packet_container_update *p)
{
    struct item *i;

    assert(p->cmd == PCK_ContainerUpdate);

    i = make_item(world, p->item.serial);
    if (i == NULL) {
        fprintf(stderr, "out of memory\n");
        return;
    }

    i->packet_container_update = *p;
}

void
world_container_content(struct world *world,
                        const struct uo_packet_container_content *p)
{
    struct item *i;
    unsigned t;

    assert(p->cmd == PCK_ContainerContent);

    for (t = 0; t < ntohs(p->num); t++) {
        i = make_item(world, p->items[t].serial);
        if (i == NULL) {
            fprintf(stderr, "out of memory\n");
            return;
        }

        i->packet_container_update.cmd = PCK_ContainerUpdate;
        i->packet_container_update.item = p->items[t];
    }
}

static void free_item(struct item *i) {
    assert(i != NULL);
    assert(i->serial != 0);

#ifndef NDEBUG
    memset(i, 0, sizeof(*i));
#endif

    free(i);
}

/** deep-delete all items contained in the specified serial */
static void
remove_item_tree(struct world *world, u_int32_t parent_serial) {
    struct item *i, *n;
    struct list_head temp;

    INIT_LIST_HEAD(&temp);

    /* move all direct children to the temporary list */
    list_for_each_entry_safe(i, n, &world->items, siblings) {
        if (i->packet_container_update.item.parent_serial == parent_serial ||
            i->packet_equip.parent_serial == parent_serial) {
            /* move to temp list */
            list_del(&i->siblings);
            list_add(&i->siblings, &temp);
        }
    }

    /* delete these, and recursively delete their children */
    list_for_each_entry_safe(i, n, &temp, siblings) {
        remove_item_tree(world, i->serial);

        list_del(&i->siblings);
        free_item(i);
    }
}

void
world_remove_item(struct world *world, u_int32_t serial)
{
    struct item *i;

    /* remove this entity */
    i = world_find_item(world, serial);
    if (i != NULL) {
        list_del(&i->siblings);
        free_item(i);
    }

    /* remove equipped items */
    remove_item_tree(world, serial);
}

void connection_delete_items(struct connection *c) {
    struct uo_packet_delete p = { .cmd = PCK_Delete };
    struct item *i, *n;
    struct linked_server *ls;

    list_for_each_entry_safe(i, n, &c->client.world.items, siblings) {
        p.serial = i->serial;

        list_for_each_entry(ls, &c->servers, siblings) {
            if (!ls->attaching)
                uo_server_send(ls->server, &p, sizeof(p));
        }

        list_del(&i->siblings);
        free(i);
    }
}

static struct mobile *
find_mobile(struct world *world, u_int32_t serial)
{
    struct mobile *mobile;

    list_for_each_entry(mobile, &world->mobiles, siblings) {
        if (mobile->serial == serial)
            return mobile;
    }

    return NULL;
}

static struct mobile *add_mobile(struct connection *c,
                                 u_int32_t serial) {
    struct mobile *m;

    m = find_mobile(&c->client.world, serial);
    if (m != NULL)
        return m;

    m = calloc(1, sizeof(*m));
    if (m == NULL) {
        fprintf(stderr, "out of memory\n");
        return NULL;
    }

    m->serial = serial;

    list_add(&m->siblings, &c->client.world.mobiles);

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

static void read_equipped(struct connection *c,
                          const struct uo_packet_mobile_incoming *p) {
    const char *p0, *i, *end;
    const struct uo_packet_fragment_mobile_item *item;
    struct uo_packet_equip equip = {
        .cmd = PCK_Equip,
        .parent_serial = p->serial,
    };

    p0 = (const char*)(const void*)p;
    end = p0 + ntohs(p->length);
    item = p->items;
    i = (const char*)(const void*)item;

    while ((const char*)(const void*)(item + 1) <= end) {
        if (item->serial == 0)
            break;
        equip.serial = item->serial;
        equip.item_id = item->item_id & htons(0x3fff);
        equip.layer = item->layer;
        if (item->item_id & htons(0x8000)) {
            equip.hue = item->hue;
            item++;
            i = (const char*)(const void*)item;
        } else {
            equip.hue = 0;
            i += sizeof(*item) - sizeof(item->hue);
            item = (const struct uo_packet_fragment_mobile_item*)i;
        }

        world_equip(&c->client.world, &equip);
    }
}

void connection_mobile_incoming(struct connection *c,
                                const struct uo_packet_mobile_incoming *p) {
    struct mobile *m;

    assert(p->cmd == PCK_MobileIncoming);

    if (p->serial == c->client.world.packet_start.serial) {
        /* update player's mobile */
        c->client.world.packet_start.body = p->body;
        c->client.world.packet_start.x = p->x;
        c->client.world.packet_start.y = p->y;
        c->client.world.packet_start.z = htons(p->z);
        c->client.world.packet_start.direction = p->direction;

        c->client.world.packet_mobile_update.body = p->body;
        c->client.world.packet_mobile_update.hue = p->hue;
        c->client.world.packet_mobile_update.x = p->x;
        c->client.world.packet_mobile_update.y = p->y;
        c->client.world.packet_mobile_update.direction = p->direction;
        c->client.world.packet_mobile_update.z = p->z;
    }

    m = add_mobile(c, p->serial);
    if (m == NULL)
        return;

    replace_packet((void**)&m->packet_mobile_incoming,
                   p, ntohs(p->length));

    read_equipped(c, p);
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
        m->packet_mobile_status->flags <= p->flags)
        replace_packet((void**)&m->packet_mobile_status,
                       p, ntohs(p->length));
}

void connection_mobile_update(struct connection *c,
                              const struct uo_packet_mobile_update *p) {
    struct mobile *m;

    if (c->client.world.packet_start.serial == p->serial) {
        /* update player's mobile */
        c->client.world.packet_mobile_update = *p;

        c->client.world.packet_start.body = p->body;
        c->client.world.packet_start.x = p->x;
        c->client.world.packet_start.y = p->y;
        c->client.world.packet_start.z = htons(p->z);
        c->client.world.packet_start.direction = p->direction;
    }

    m = find_mobile(&c->client.world, p->serial);
    if (m == NULL) {
        fprintf(stderr, "warning in connection_mobile_update: no such mobile 0x%x\n",
                ntohl(p->serial));
        return;
    }

    /* copy values to m->packet_mobile_incoming */
    if (m->packet_mobile_incoming != NULL) {
        m->packet_mobile_incoming->body = p->body;
        m->packet_mobile_incoming->x = p->x;
        m->packet_mobile_incoming->y = p->y;
        m->packet_mobile_incoming->z = p->z;
        m->packet_mobile_incoming->direction = p->direction;
        m->packet_mobile_incoming->hue = p->hue;
    }
}

void connection_mobile_moving(struct connection *c,
                              const struct uo_packet_mobile_moving *p) {
    struct mobile *m;

    if (c->client.world.packet_start.serial == p->serial) {
        /* update player's mobile */
        c->client.world.packet_start.body = p->body;
        c->client.world.packet_start.x = p->x;
        c->client.world.packet_start.y = p->y;
        c->client.world.packet_start.z = htons(p->z);
        c->client.world.packet_start.direction = p->direction;

        c->client.world.packet_mobile_update.body = p->body;
        c->client.world.packet_mobile_update.hue = p->hue;
        c->client.world.packet_mobile_update.x = p->x;
        c->client.world.packet_mobile_update.y = p->y;
        c->client.world.packet_mobile_update.direction = p->direction;
        c->client.world.packet_mobile_update.z = p->z;
    }

    m = find_mobile(&c->client.world, p->serial);
    if (m == NULL) {
        fprintf(stderr, "warning in connection_mobile_moving: no such mobile 0x%x\n",
                ntohl(p->serial));
        return;
    }

    /* copy values to m->packet_mobile_incoming */
    if (m->packet_mobile_incoming != NULL) {
        m->packet_mobile_incoming->body = p->body;
        m->packet_mobile_incoming->x = p->x;
        m->packet_mobile_incoming->y = p->y;
        m->packet_mobile_incoming->z = p->z;
        m->packet_mobile_incoming->direction = p->direction;
        m->packet_mobile_incoming->hue = p->hue;
        m->packet_mobile_incoming->flags = p->flags;
        m->packet_mobile_incoming->notoriety = p->notoriety;
    }
}

void connection_mobile_zone(struct connection *c,
                            const struct uo_packet_zone_change *p) {
    c->client.world.packet_start.x = p->x;
    c->client.world.packet_start.y = p->y;
    c->client.world.packet_start.z = p->z;

    c->client.world.packet_mobile_update.x = p->x;
    c->client.world.packet_mobile_update.y = p->y;
    c->client.world.packet_mobile_update.z = ntohs(p->z);
}

static void free_mobile(struct mobile *m) {
    assert(m != NULL);
    assert(m->serial != 0);

    if (m->packet_mobile_incoming != NULL)
        free(m->packet_mobile_incoming);
    if (m->packet_mobile_status != NULL)
        free(m->packet_mobile_status);

#ifndef NDEBUG
    memset(m, 0, sizeof(*m));
#endif

    free(m);
}

void
world_remove_mobile(struct world *world, u_int32_t serial) {
    struct mobile *m;

    /* remove this entity */
    m = find_mobile(world, serial);
    if (m != NULL) {
        list_del(&m->siblings);
        free_mobile(m);
    }

    /* remove equipped items */
    remove_item_tree(world, serial);
}

void connection_delete_mobiles(struct connection *c) {
    struct uo_packet_delete p = { .cmd = PCK_Delete };
    struct mobile *m, *n;
    struct linked_server *ls;

    list_for_each_entry_safe(m, n, &c->client.world.mobiles, siblings) {
        p.serial = m->serial;

        list_for_each_entry(ls, &c->servers, siblings) {
            if (!ls->attaching)
                uo_server_send(ls->server, &p, sizeof(p));
        }

        list_del(&m->siblings);
        free_mobile(m);
    }
}

void
world_remove_serial(struct world *world, u_int32_t serial) {
    u_int32_t host_serial = ntohl(serial);

    if (host_serial < 0x40000000)
        world_remove_mobile(world, serial);
    else if (host_serial < 0x80000000)
        world_remove_item(world, serial);
}

void
world_walked(struct world *world, u_int16_t x, u_int16_t y,
             u_int8_t direction, u_int8_t notoriety) {
    struct mobile *m;

    world->packet_start.x = x;
    world->packet_start.y = y;
    world->packet_start.direction = direction;

    world->packet_mobile_update.x = x;
    world->packet_mobile_update.y = y;
    world->packet_mobile_update.direction = direction;

    m = find_mobile(world, world->packet_start.serial);
    if (m != NULL && m->packet_mobile_incoming != NULL) {
        m->packet_mobile_incoming->x = x;
        m->packet_mobile_incoming->y = y;
        m->packet_mobile_incoming->direction = direction;
        m->packet_mobile_incoming->notoriety = notoriety;
    }
}
