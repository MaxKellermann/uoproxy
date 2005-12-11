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

#ifndef __CONNECTION_H
#define __CONNECTION_H

#include "packets.h"

#define MAX_CHARACTERS 16

struct selectx;
struct instance;
struct addrinfo;

struct item {
    struct item *next;
    u_int32_t serial;
    struct uo_packet_world_item packet_world_item;
};

struct mobile {
    struct mobile *next;
    u_int32_t serial;
    struct uo_packet_mobile_incoming *packet_mobile_incoming;
    struct uo_packet_mobile_status *packet_mobile_status;
};

struct linked_server {
    struct linked_server *next;
    struct uo_server *server;
    int invalid, welcome, attaching;
};

struct connection {
    /* linked list and parent */
    struct connection *next;
    struct instance *instance;

    /* flags */
    int invalid, background;

    int in_game;

    /* reconnect */
    int autoreconnect, reconnecting;
    struct addrinfo *server_address;

    /* state */
    char username[30], password[30];

    struct uo_fragment_character_info characters[MAX_CHARACTERS];
    unsigned num_characters, character_index;

    u_int16_t supported_features_flags;
    struct uo_packet_start packet_start;
    struct uo_packet_map_change packet_map_change;
    struct uo_packet_map_patches packet_map_patches;
    struct uo_packet_season packet_season;
    struct uo_packet_mobile_update packet_mobile_update;
    struct uo_packet_global_light_level packet_global_light_level;
    struct uo_packet_personal_light_level packet_personal_light_level;
    struct uo_packet_war_mode packet_war_mode;
    unsigned char ping_request, ping_ack;
    struct item *items_head;
    struct mobile *mobiles_head;

    time_t next_ping, next_reconnect;

    /* sub-objects */
    struct uo_client *client;
    struct linked_server *servers_head;
    struct linked_server *current_server;
};

int connection_new(struct instance *instance,
                   int server_socket,
                   struct connection **connectionp);

void connection_delete(struct connection *c);

#ifdef NDEBUG
#define connection_check(c) do { (void)(c); } while (0)
#else
void connection_check(const struct connection *c);
#endif

void connection_invalidate(struct connection *c);

struct linked_server *connection_add_server(struct connection *c, struct uo_server *server);

void connection_pre_select(struct connection *c, struct selectx *sx);

int connection_post_select(struct connection *c, struct selectx *sx);

void connection_idle(struct connection *c, time_t now);

void connection_speak_console(struct connection *c, const char *msg);

void connection_broadcast_servers_except(struct connection *c,
                                         const void *data, size_t length,
                                         struct uo_server *except);

/* state */

void connection_world_item(struct connection *c,
                           const struct uo_packet_world_item *p);

void connection_remove_item(struct connection *c, u_int32_t serial);

void connection_delete_items(struct connection *c);

void connection_mobile_incoming(struct connection *c,
                                const struct uo_packet_mobile_incoming *p);

void connection_mobile_status(struct connection *c,
                              const struct uo_packet_mobile_status *p);

void connection_mobile_update(struct connection *c,
                              const struct uo_packet_mobile_update *p);

void connection_mobile_moving(struct connection *c,
                              const struct uo_packet_mobile_moving *p);

void connection_mobile_zone(struct connection *c,
                            const struct uo_packet_zone_change *p);

void connection_remove_mobile(struct connection *c, u_int32_t serial);

void connection_delete_mobiles(struct connection *c);

void connection_remove_serial(struct connection *c, u_int32_t serial);

/* reconnect */

void connection_disconnect(struct connection *c);

void connection_reconnect(struct connection *c);

/* attach */

void attach_after_game_login(struct connection *c,
                             struct uo_server *server);

void attach_after_play_character(struct connection *c,
                                 struct linked_server *ls);

/* command */

void connection_handle_command(struct connection *c,
                               struct linked_server *server,
                               const char *command);

#endif
