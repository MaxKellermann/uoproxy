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

#include "Connection.hxx"
#include "LinkedServer.hxx"
#include "Instance.hxx"
#include "Server.hxx"
#include "Bridge.hxx"
#include "Log.hxx"

#include <assert.h>
#include <string.h>

static void
attach_item(LinkedServer *ls,
            Item *item)
{
    World *world = &ls->connection->client.world;

    item->attach_sequence = world->item_attach_sequence;

    switch (item->socket.cmd) {
        uint32_t parent_serial;
        Item *parent;

    case PCK_WorldItem:
        if (ls->client_version.protocol >= PROTOCOL_7) {
            uo_server_send(ls->server, &item->socket.ground,
                           sizeof(item->socket.ground));
        } else {
            struct uo_packet_world_item p;
            world_item_from_7(&p, &item->socket.ground);
            uo_server_send(ls->server, &p, p.length);
        }

        break;

    case PCK_ContainerUpdate:
        /* attach parent first */
        parent_serial = item->socket.container.item.parent_serial;
        parent = world->FindItem(parent_serial);
        if (parent != nullptr &&
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
attach_send_world(LinkedServer *ls)
{
    World *world = &ls->connection->client.world;
    struct uo_packet_login_complete login_complete;

    /* 0x1b LoginConfirm */
    if (world->packet_start.cmd == PCK_Start)
        uo_server_send(ls->server, &world->packet_start,
                       sizeof(world->packet_start));

    /* 0xbf 0x08 MapChange */
    if (world->packet_map_change.length > 0) {
        assert(world->packet_map_change.cmd == PCK_Extended);
        assert(world->packet_map_change.length == sizeof(world->packet_map_change));
        assert(world->packet_map_change.extended_cmd == 0x0008);
        uo_server_send(ls->server, &world->packet_map_change,
                       world->packet_map_change.length);
    }

    /* 0xbf 0x18 MapPatches */
    if (world->packet_map_patches.length > 0) {
        assert(world->packet_map_patches.cmd == PCK_Extended);
        assert(world->packet_map_patches.length == sizeof(world->packet_map_patches));
        assert(world->packet_map_patches.extended_cmd == 0x0018);
        uo_server_send(ls->server, &world->packet_map_patches,
                       world->packet_map_patches.length);
    }

    /* 0xbc SeasonChange */
    if (world->packet_season.cmd == PCK_Season)
        uo_server_send(ls->server, &world->packet_season,
                       sizeof(world->packet_season));

    /* 0xb9 SupportedFeatures */
    if (ls->client_version.protocol >= PROTOCOL_6_0_14) {
        struct uo_packet_supported_features_6014 supported_features;
        supported_features.cmd = PCK_SupportedFeatures;
        supported_features.flags = ls->connection->client.supported_features_flags;
        uo_server_send(ls->server, &supported_features,
                       sizeof(supported_features));
    } else {
        struct uo_packet_supported_features supported_features;
        supported_features.cmd = PCK_SupportedFeatures;
        supported_features.flags = ls->connection->client.supported_features_flags;
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
    for (const auto &mobile : world->mobiles) {
        if (mobile.packet_mobile_incoming != nullptr)
            uo_server_send(ls->server, mobile.packet_mobile_incoming,
                           mobile.packet_mobile_incoming->length);
        if (mobile.packet_mobile_status != nullptr)
            uo_server_send(ls->server, mobile.packet_mobile_status,
                           mobile.packet_mobile_status->length);
    }

    /* items */
    ++world->item_attach_sequence;
    for (auto &item : world->items)
        if (item.attach_sequence != world->item_attach_sequence)
            attach_item(ls, &item);

    /* LoginComplete */
    login_complete.cmd = PCK_ReDrawAll;
    uo_server_send(ls->server, &login_complete,
                   sizeof(login_complete));

}

void
attach_after_play_server(Connection *c, LinkedServer *ls)
{
    assert(c->IsInGame());

    LogFormat(2, "attaching connection\n");

    c->Add(*ls);

    attach_send_world(ls);
}
