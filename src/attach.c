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
#include <stdio.h>
#include <string.h>

#include "connection.h"
#include "instance.h"
#include "server.h"

struct connection *find_attach_connection(struct connection *c) {
    struct connection *c2;

    for (c2 = c->instance->connections_head; c2 != NULL; c2 = c2->next)
        if (c2 != c && c2->in_game && c2->packet_start.serial != 0 &&
            memcmp(c->username, c2->username, sizeof(c->username)) == 0 &&
            memcmp(c->password, c2->password, sizeof(c->password)) == 0 &&
            c->server_index == c2->server_index &&
            c2->num_characters > 0)
            return c2;

    return NULL;
}

void attach_after_play_server(struct connection *c,
                              struct uo_server *server) {
    struct linked_server *ls;
    struct uo_packet_supported_features supported_features;
    struct uo_packet_simple_character_list character_list;

    assert(c->in_game);
    connection_check(c);

    printf("attaching connection\n");

    ls = connection_add_server(c, server);
    if (ls == NULL) {
        uo_server_dispose(server);
        fprintf(stderr, "out of memory\n");
        return;
    }

    ls->attaching = 1;

    supported_features.cmd = PCK_SupportedFeatures;
    supported_features.flags = c->supported_features_flags;
    uo_server_send(ls->server, &supported_features,
                   sizeof(supported_features));

    memset(&character_list, 0, sizeof(character_list));
    character_list.cmd = PCK_CharList;
    character_list.length = htons(sizeof(character_list));
    character_list.character_count = 1;
    character_list.character_info[0] = c->characters[c->character_index];
    character_list.city_count = 0;
    character_list.flags = htonl(0x14);
    uo_server_send(ls->server, &character_list,
                   sizeof(character_list));

    /*
      uo_server_send(c2->server, &c2->packet_start,
      sizeof(c2->packet_start));
    */
}

static void attach_item(struct connection *c,
                        struct linked_server *ls,
                        struct item *item) {
    item->attach_sequence = c->item_attach_sequence;

    if (item->packet_container_update.cmd == PCK_ContainerUpdate) {
        /* attach parent first */
        u_int32_t parent_serial
            = item->packet_container_update.item.parent_serial;
        struct item *parent = connection_find_item(c, parent_serial);
        if (parent != NULL &&
            parent->attach_sequence != c->item_attach_sequence)
            attach_item(c, ls, parent);

        /* then this item as container content */
        uo_server_send(ls->server, &item->packet_container_update,
                       sizeof(item->packet_container_update));
    }

    if (item->packet_container_open.cmd == PCK_ContainerOpen)
        uo_server_send(ls->server, &item->packet_container_open,
                       sizeof(item->packet_container_open));

    if (item->packet_world_item.cmd == PCK_WorldItem)
        uo_server_send(ls->server, &item->packet_world_item,
                       ntohs(item->packet_world_item.length));
    if (item->packet_equip.cmd == PCK_Equip)
        uo_server_send(ls->server, &item->packet_equip,
                       sizeof(item->packet_equip));
}

void attach_after_play_character(struct connection *c,
                                 struct linked_server *ls) {
    struct uo_packet_supported_features supported_features;
    struct uo_packet_login_complete login_complete;
    struct mobile *mobile;
    struct item *item;

    connection_check(c);

    if (ls->invalid)
        return;

    assert(ls->server != NULL);
    assert(ls->attaching);

    /* 0x1b LoginConfirm */
    if (c->packet_start.cmd == PCK_Start)
        uo_server_send(ls->server, &c->packet_start,
                       sizeof(c->packet_start));

    /* 0xbf 0x08 MapChange */
    if (ntohs(c->packet_map_change.length) > 0) {
        assert(c->packet_map_change.cmd == PCK_Extended);
        assert(ntohs(c->packet_map_change.length) == sizeof(c->packet_map_change));
        assert(ntohs(c->packet_map_change.extended_cmd) == 0x0008);
        uo_server_send(ls->server, &c->packet_map_change,
                       ntohs(c->packet_map_change.length));
    }

    /* 0xbf 0x18 MapPatches */
    if (ntohs(c->packet_map_patches.length) > 0) {
        assert(c->packet_map_patches.cmd == PCK_Extended);
        assert(ntohs(c->packet_map_patches.length) == sizeof(c->packet_map_patches));
        assert(ntohs(c->packet_map_patches.extended_cmd) == 0x0018);
        uo_server_send(ls->server, &c->packet_map_patches,
                       ntohs(c->packet_map_patches.length));
    }

    /* 0xbc SeasonChange */
    if (c->packet_season.cmd == PCK_Season)
        uo_server_send(ls->server, &c->packet_season, sizeof(c->packet_season));

    /* 0xb9 SupportedFeatures */
    supported_features.cmd = PCK_SupportedFeatures;
    supported_features.flags = c->supported_features_flags;
    uo_server_send(ls->server, &supported_features,
                   sizeof(supported_features));

    /* 0x4f GlobalLightLevel */
    if (c->packet_global_light_level.cmd == PCK_GlobalLightLevel)
        uo_server_send(ls->server, &c->packet_global_light_level,
                       sizeof(c->packet_global_light_level));

    /* 0x4e PersonalLightLevel */
    if (c->packet_personal_light_level.cmd == PCK_PersonalLightLevel)
        uo_server_send(ls->server, &c->packet_personal_light_level,
                       sizeof(c->packet_personal_light_level));

    /* 0x20 MobileUpdate */
    if (c->packet_mobile_update.cmd == PCK_MobileUpdate)
        uo_server_send(ls->server, &c->packet_mobile_update,
                       sizeof(c->packet_mobile_update));

    /* WarMode */
    if (c->packet_war_mode.cmd == PCK_WarMode)
        uo_server_send(ls->server, &c->packet_war_mode,
                       sizeof(c->packet_war_mode));

    /* mobiles */
    for (mobile = c->mobiles_head; mobile != NULL; mobile = mobile->next) {
        if (mobile->packet_mobile_incoming != NULL)
            uo_server_send(ls->server, mobile->packet_mobile_incoming,
                           ntohs(mobile->packet_mobile_incoming->length));
        if (mobile->packet_mobile_status != NULL)
            uo_server_send(ls->server, mobile->packet_mobile_status,
                           ntohs(mobile->packet_mobile_status->length));
    }

    /* items */
    ++c->item_attach_sequence;
    for (item = c->items_head; item != NULL; item = item->next) {
        if (item->attach_sequence != c->item_attach_sequence)
            attach_item(c, ls, item);
    }

    /* LoginComplete */
    login_complete.cmd = PCK_ReDrawAll;
    uo_server_send(ls->server, &login_complete,
                   sizeof(login_complete));

    ls->attaching = 0;
}
