/*
 * uoproxy
 *
 * (c) 2005-2010 Max Kellermann <max@duempel.org>
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

#include "connection.h"
#include "instance.h"
#include "server.h"
#include "bridge.h"
#include "log.h"

#include <assert.h>
#include <string.h>

struct connection *find_attach_connection(struct connection *c) {
    struct connection *c2;

    list_for_each_entry(c2, &c->instance->connections, siblings)
        if (c2 != c && c2->in_game &&
            c2->client.world.packet_start.serial != 0 &&
            strncmp(c->username, c2->username, sizeof(c->username)) == 0 &&
            strncmp(c->password, c2->password, sizeof(c->password)) == 0 &&
            c->server_index == c2->server_index &&
            c2->client.num_characters > 0)
            return c2;

    return NULL;
}

static void
attach_item(struct linked_server *ls,
            struct item *item)
{
    struct world *world = &ls->connection->client.world;

    item->attach_sequence = world->item_attach_sequence;

    switch (item->socket.cmd) {
        uint32_t parent_serial;

    case PCK_WorldItem:
        if (ls->client_version.protocol >= PROTOCOL_7) {
            uo_server_send(ls->server, &item->socket.ground,
                           sizeof(item->socket.ground));
        } else {
            struct uo_packet_world_item p;
            world_item_from_7(&p, &item->socket.ground);
            uo_server_send(ls->server, &p, ntohs(p.length));
        }

        break;

    case PCK_ContainerUpdate:
        /* attach parent first */
        parent_serial = item->socket.container.item.parent_serial;
        struct item *parent = world_find_item(world, parent_serial);
        if (parent != NULL &&
            parent->attach_sequence != world->item_attach_sequence)
            attach_item(ls, parent);

        /* then this item as container content */

        if (ls->client_version.protocol < PROTOCOL_6) {
            /* convert to v5 packet */
            struct uo_packet_container_update p5;

            container_update_6_to_5(&p5, &item->socket.container);
            uo_server_send(ls->server, &p5, sizeof(p5));
        } else {
            uo_server_send(ls->server, &item->socket.container,
                           sizeof(item->socket.container));
        }

        break;

    case PCK_Equip:
        uo_server_send(ls->server, &item->socket.mobile,
                       sizeof(item->socket.mobile));
        break;
    }

    if (item->packet_container_open.cmd == PCK_ContainerOpen) {
        if (ls->client_version.protocol >= PROTOCOL_7) {
            struct uo_packet_container_open_7 p7 = {
                .base = item->packet_container_open,
                .zero = 0x00,
                .x7d = 0x7d,
            };

            uo_server_send(ls->server, &p7, sizeof(p7));
        } else
            uo_server_send(ls->server, &item->packet_container_open,
                           sizeof(item->packet_container_open));
    }
}

void
attach_send_world(struct linked_server *ls)
{
    struct world *world = &ls->connection->client.world;
    struct uo_packet_login_complete login_complete;
    struct mobile *mobile;
    struct item *item;

    /* 0x1b LoginConfirm */
    if (world->packet_start.cmd == PCK_Start)
        uo_server_send(ls->server, &world->packet_start,
                       sizeof(world->packet_start));

    /* 0xbf 0x08 MapChange */
    if (ntohs(world->packet_map_change.length) > 0) {
        assert(world->packet_map_change.cmd == PCK_Extended);
        assert(ntohs(world->packet_map_change.length) == sizeof(world->packet_map_change));
        assert(ntohs(world->packet_map_change.extended_cmd) == 0x0008);
        uo_server_send(ls->server, &world->packet_map_change,
                       ntohs(world->packet_map_change.length));
    }

    /* 0xbf 0x18 MapPatches */
    if (ntohs(world->packet_map_patches.length) > 0) {
        assert(world->packet_map_patches.cmd == PCK_Extended);
        assert(ntohs(world->packet_map_patches.length) == sizeof(world->packet_map_patches));
        assert(ntohs(world->packet_map_patches.extended_cmd) == 0x0018);
        uo_server_send(ls->server, &world->packet_map_patches,
                       ntohs(world->packet_map_patches.length));
    }

    /* 0xbc SeasonChange */
    if (world->packet_season.cmd == PCK_Season)
        uo_server_send(ls->server, &world->packet_season,
                       sizeof(world->packet_season));

    /* 0xb9 SupportedFeatures */
    if (ls->client_version.protocol >= PROTOCOL_6_0_14) {
        struct uo_packet_supported_features_6014 supported_features;
        supported_features.cmd = PCK_SupportedFeatures;
        supported_features.flags = htonl(ls->connection->client.supported_features_flags);
        uo_server_send(ls->server, &supported_features,
                       sizeof(supported_features));
    } else {
        struct uo_packet_supported_features supported_features;
        supported_features.cmd = PCK_SupportedFeatures;
        supported_features.flags = htons(ls->connection->client.supported_features_flags);
        uo_server_send(ls->server, &supported_features,
                       sizeof(supported_features));
    }

    /* 0x4f GlobalLightLevel */
    if (world->packet_global_light_level.cmd == PCK_GlobalLightLevel)
        uo_server_send(ls->server, &world->packet_global_light_level,
                       sizeof(world->packet_global_light_level));

    /* 0x4e PersonalLightLevel */
    if (world->packet_personal_light_level.cmd == PCK_PersonalLightLevel)
        uo_server_send(ls->server, &world->packet_personal_light_level,
                       sizeof(world->packet_personal_light_level));

    /* 0x20 MobileUpdate */
    if (world->packet_mobile_update.cmd == PCK_MobileUpdate)
        uo_server_send(ls->server, &world->packet_mobile_update,
                       sizeof(world->packet_mobile_update));

    /* WarMode */
    if (world->packet_war_mode.cmd == PCK_WarMode)
        uo_server_send(ls->server, &world->packet_war_mode,
                       sizeof(world->packet_war_mode));

    /* mobiles */
    list_for_each_entry(mobile, &world->mobiles, siblings) {
        if (mobile->packet_mobile_incoming != NULL)
            uo_server_send(ls->server, mobile->packet_mobile_incoming,
                           ntohs(mobile->packet_mobile_incoming->length));
        if (mobile->packet_mobile_status != NULL)
            uo_server_send(ls->server, mobile->packet_mobile_status,
                           ntohs(mobile->packet_mobile_status->length));
    }

    /* items */
    ++world->item_attach_sequence;
    list_for_each_entry(item, &world->items, siblings)
        if (item->attach_sequence != world->item_attach_sequence)
            attach_item(ls, item);

    /* LoginComplete */
    login_complete.cmd = PCK_ReDrawAll;
    uo_server_send(ls->server, &login_complete,
                   sizeof(login_complete));

}

void
attach_after_play_server(struct connection *c, struct linked_server *ls)
{
    assert(c->in_game);
    connection_check(c);

    log(2, "attaching connection\n");

    connection_server_add(c, ls);

    attach_send_world(ls);
}
