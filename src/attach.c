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
#include "server.h"

void attach_after_game_login(struct connection *old,
                             struct connection *new) {
    struct uo_packet_supported_features supported_features;
    struct uo_packet_simple_character_list character_list;

    assert(old->server == NULL);

    printf("attaching connection\n");
    old->server = new->server;
    old->attaching = 1;
    new->welcome = 0;
    new->server = NULL;

    supported_features.cmd = PCK_SupportedFeatures;
    supported_features.flags = old->supported_features_flags;
    uo_server_send(old->server, &supported_features,
                   sizeof(supported_features));

    memset(&character_list, 0, sizeof(character_list));
    character_list.cmd = PCK_CharList;
    character_list.length = htons(sizeof(character_list));
    character_list.character_count = 1;
    strcpy(character_list.character_info[0].name, "<attach>");
    character_list.city_count = 0;
    character_list.flags = htonl(0x14);
    uo_server_send(old->server, &character_list,
                   sizeof(character_list));

    /*
      uo_server_send(c2->server, &c2->packet_start,
      sizeof(c2->packet_start));
    */
}

void attach_after_play_character(struct connection *c) {
    struct uo_packet_supported_features supported_features;
    struct uo_packet_login_complete login_complete;
    struct mobile *mobile;
    struct item *item;

    if (c->server == NULL)
        return;

    /* 0x1b LoginConfirm */
    assert(c->packet_start.cmd == PCK_Start);
    if (c->packet_start.cmd == PCK_Start)
        uo_server_send(c->server, &c->packet_start,
                       sizeof(c->packet_start));

    /* 0xbf 0x08 MapChange */
    assert(ntohs(c->packet_map_change.length) == sizeof(c->packet_map_change));
    assert(c->packet_map_change.cmd == PCK_ExtData);
    assert(ntohs(c->packet_map_change.extended_cmd) == 0x0008);
    if (ntohs(c->packet_map_change.length) > 0)
        uo_server_send(c->server, &c->packet_map_change,
                       ntohs(c->packet_map_change.length));

    /* 0xbf 0x18 MapPatches */
    assert(ntohs(c->packet_map_patches.length) == sizeof(c->packet_map_patches));
    assert(c->packet_map_patches.cmd == PCK_ExtData);
    assert(ntohs(c->packet_map_patches.extended_cmd) == 0x0018);
    if (ntohs(c->packet_map_patches.length) > 0)
        uo_server_send(c->server, &c->packet_map_patches,
                       ntohs(c->packet_map_patches.length));

    /* 0xbc SeasonChange */
    assert(c->packet_season.cmd == PCK_Season);
    if (c->packet_season.cmd == PCK_Season)
        uo_server_send(c->server, &c->packet_season, sizeof(c->packet_season));

    /* 0xb9 SupportedFeatures */
    supported_features.cmd = PCK_SupportedFeatures;
    supported_features.flags = c->supported_features_flags;
    uo_server_send(c->server, &supported_features,
                   sizeof(supported_features));

    /* 0x20 MobileUpdate */
    assert(c->packet_mobile_update.cmd == PCK_MobileUpdate);
    if (c->packet_mobile_update.cmd == PCK_MobileUpdate) {
        uo_server_send(c->server, &c->packet_mobile_update,
                       sizeof(c->packet_mobile_update));
        uo_server_send(c->server, &c->packet_mobile_update,
                       sizeof(c->packet_mobile_update));
    }

    /* 0x4f GlobalLightLevel */
    assert(c->packet_global_light_level.cmd == PCK_GlobalLightLevel);
    if (c->packet_global_light_level.cmd == PCK_GlobalLightLevel)
        uo_server_send(c->server, &c->packet_global_light_level,
                       sizeof(c->packet_global_light_level));

    /* 0x4e PersonalLightLevel */
    assert(c->packet_personal_light_level.cmd == PCK_PersonalLightLevel);
    if (c->packet_personal_light_level.cmd == PCK_PersonalLightLevel)
        uo_server_send(c->server, &c->packet_personal_light_level,
                       sizeof(c->packet_personal_light_level));

    /* 0x20 MobileUpdate */
    assert(c->packet_mobile_update.cmd == PCK_MobileUpdate);
    if (c->packet_mobile_update.cmd == PCK_MobileUpdate)
        uo_server_send(c->server, &c->packet_mobile_update,
                       sizeof(c->packet_mobile_update));

    /* WarMode */
    assert(c->packet_war_mode.cmd == PCK_WarMode);
    if (c->packet_war_mode.cmd == PCK_WarMode)
        uo_server_send(c->server, &c->packet_war_mode,
                       sizeof(c->packet_war_mode));

    /* mobiles */
    for (mobile = c->mobiles_head; mobile != NULL; mobile = mobile->next) {
        if (mobile->packet_mobile_incoming != NULL)
            uo_server_send(c->server, mobile->packet_mobile_incoming,
                           ntohs(mobile->packet_mobile_incoming->length));
        if (mobile->packet_mobile_status != NULL)
            uo_server_send(c->server, mobile->packet_mobile_status,
                           ntohs(mobile->packet_mobile_status->length));
    }

    /* items */
    for (item = c->items_head; item != NULL; item = item->next) {
        uo_server_send(c->server, &item->packet_put,
                       ntohs(item->packet_put.length));
    }

    /* LoginComplete */
    login_complete.cmd = PCK_ReDrawAll;
    uo_server_send(c->server, &login_complete,
                   sizeof(login_complete));

    c->attaching = 0;
}
