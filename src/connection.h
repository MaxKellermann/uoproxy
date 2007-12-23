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

#ifndef __CONNECTION_H
#define __CONNECTION_H

#include "packets.h"
#include "list.h"

#include <event.h>

#define MAX_CHARACTERS 16
#define MAX_WALK_QUEUE 4

struct selectx;
struct instance;
struct addrinfo;

struct item {
    struct list_head siblings;

    u_int32_t serial;
    struct uo_packet_world_item packet_world_item;
    struct uo_packet_equip packet_equip;
    struct uo_packet_container_open packet_container_open;
    struct uo_packet_container_update packet_container_update;
    unsigned attach_sequence;
};

struct mobile {
    struct list_head siblings;

    u_int32_t serial;
    struct uo_packet_mobile_incoming *packet_mobile_incoming;
    struct uo_packet_mobile_status *packet_mobile_status;
};

struct linked_server {
    struct list_head siblings;

    struct connection *connection;

    struct uo_server *server;
    int welcome, attaching;
};

struct connection_walk_item {
    struct uo_packet_walk packet;
    u_int8_t seq;
};

struct connection_walk_state {
    struct linked_server *server;
    struct connection_walk_item queue[MAX_WALK_QUEUE];
    unsigned queue_size;
    u_int8_t seq_next;
};

struct connection {
    /* linked list and parent */
    struct list_head siblings;

    struct instance *instance;

    /* flags */
    int background;

    int in_game;

    /* reconnect */
    int autoreconnect, reconnecting;
    struct event reconnect_event;

    /* state */
    char username[30], password[30];

    unsigned server_index;

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
    struct uo_packet_target packet_target;
    unsigned char ping_request, ping_ack;
    struct list_head items;
    unsigned item_attach_sequence;

    struct list_head mobiles;

    struct connection_walk_state walk;

    /* client version */

    struct uo_packet_client_version *client_version;

    /* sub-objects */
    struct uo_client *client;
    struct event ping_event;

    struct list_head servers;
    struct linked_server *current_server;
};

int connection_new(struct instance *instance,
                   int server_socket,
                   struct connection **connectionp);

void connection_delete(struct connection *c);

#ifdef NDEBUG
static inline void
connection_check(const struct connection *c)
{
    (void)c;
}
#else
void connection_check(const struct connection *c);
#endif

void connection_invalidate(struct connection *c);

void connection_speak_console(struct connection *c, const char *msg);

void connection_broadcast_servers_except(struct connection *c,
                                         const void *data, size_t length,
                                         struct uo_server *except);


/* server list */

void
connection_server_add(struct connection *c, struct linked_server *ls);

void
connection_server_remove(struct connection *c, struct linked_server *ls);

struct linked_server *
connection_server_new(struct connection *c, int fd);

void
connection_server_dispose(struct connection *c, struct linked_server *ls);


/* state */

struct item *connection_find_item(struct connection *c,
                                  u_int32_t serial);

void connection_world_item(struct connection *c,
                           const struct uo_packet_world_item *p);

void connection_equip(struct connection *c,
                      const struct uo_packet_equip *p);

void connection_container_open(struct connection *c,
                               const struct uo_packet_container_open *p);

void connection_container_update(struct connection *c,
                                 const struct uo_packet_container_update *p);

void connection_container_content(struct connection *c,
                                  const struct uo_packet_container_content *p);

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

void connection_walked(struct connection *c, u_int16_t x, u_int16_t y,
                       u_int8_t direction, u_int8_t notoriety);

/* walk */

void connection_walk_server_removed(struct connection_walk_state *state,
                                    struct linked_server *server);

void
connection_walk_request(struct connection *c,
                        struct linked_server *server,
                        const struct uo_packet_walk *p);

void connection_walk_cancel(struct connection *c,
                            const struct uo_packet_walk_cancel *p);

void connection_walk_ack(struct connection *c,
                         const struct uo_packet_walk_ack *p);

/* reconnect */

int
connection_client_connect(struct connection *c,
                          const struct addrinfo *server_address,
                          u_int32_t seed);

void connection_disconnect(struct connection *c);

void connection_reconnect(struct connection *c);

void
connection_reconnect_delayed(struct connection *c);

/* attach */

struct connection *find_attach_connection(struct connection *c);

void attach_after_play_server(struct connection *c,
                              struct linked_server *ls);

void attach_after_play_character(struct connection *c,
                                 struct linked_server *ls);

/* command */

void connection_handle_command(struct connection *c,
                               struct linked_server *server,
                               const char *command);

#endif
