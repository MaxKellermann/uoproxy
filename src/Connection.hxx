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

#ifndef __CONNECTION_H
#define __CONNECTION_H

#include "packets.h"
#include "World.hxx"
#include "CVersion.hxx"

#include <event.h>

#define MAX_CHARACTERS 16
#define MAX_WALK_QUEUE 4

struct Instance;
struct Connection;

struct StatefulClient {
    bool reconnecting = false, version_requested = false;
    struct event reconnect_event;

    struct uo_client *client = nullptr;
    struct event ping_event;

    struct uo_fragment_character_info characters[MAX_CHARACTERS];
    unsigned num_characters = 0;

    uint32_t supported_features_flags = 0;

    unsigned char ping_request = 0, ping_ack = 0;

    World world;

    StatefulClient() noexcept {
        evtimer_set(&ping_event, nullptr, nullptr);
    }

    ~StatefulClient() noexcept {
        if (reconnecting)
            event_del(&reconnect_event);
    }

    StatefulClient(const StatefulClient &) = delete;
    StatefulClient &operator=(const StatefulClient &) = delete;
};

struct LinkedServer {
    struct list_head siblings;

    Connection *connection;

    struct uo_server *server = nullptr;
    bool welcome = false, attaching = false;

    struct client_version client_version;

    /** Razor_workaround support here: we save the charlist until
        the client says gamelogin, at which point we turn compression on in our
        emulated server and send a charlist. */
    bool expecting_reconnect, got_gamelogin;
    struct uo_packet_simple_character_list *enqueued_charlist;

    bool is_zombie; /**< zombie handling */
    struct event zombie_timeout; /**< zombies time out and auto-reap themselves
                                      after 5 seconds using this timer */
    uint32_t auth_id; /**< unique identifier for this linked_server used in
                           redirect handling to locate the zombied
                           linked_server */

    LinkedServer() = default;
    ~LinkedServer() noexcept;

    LinkedServer(const LinkedServer &) = delete;
    LinkedServer &operator=(const LinkedServer &) = delete;
};

struct WalkState {
    struct Item {
        /**
         * The walk packet sent by the client.
         */
        struct uo_packet_walk packet;

        /**
         * The walk sequence number which was sent to the server.
         */
        uint8_t seq;
    };

    LinkedServer *server = nullptr;
    Item queue[MAX_WALK_QUEUE];
    unsigned queue_size = 0;
    uint8_t seq_next = 0;
};

struct Connection {
    /* linked list and parent */
    struct list_head siblings;

    Instance *const instance;

    /* flags */
    const bool background;

    bool in_game = false;

    /* reconnect */
    const bool autoreconnect;

    /* client stuff (= connection to server) */

    StatefulClient client;

    /* state */
    char username[30]{}, password[30]{};

    unsigned server_index = 0;

    unsigned character_index = 0;

    WalkState walk;

    /* client version */

    struct client_version client_version;

    /* sub-objects */

    struct list_head servers;

    Connection(Instance &_instance, bool _background,
               bool _autoreconnect)
        :instance(&_instance), background(_background),
         autoreconnect(_autoreconnect)
    {
        INIT_LIST_HEAD(&servers);
    }

    ~Connection() noexcept;

    Connection(const Connection &) = delete;
    Connection &operator=(const Connection &) = delete;
};

int connection_new(Instance *instance,
                   int server_socket,
                   Connection **connectionp);

#ifdef NDEBUG
static inline void
connection_check(const Connection *c)
{
    (void)c;
}
#else
void connection_check(const Connection *c);
#endif

void connection_delete(Connection *c);

void connection_speak_console(Connection *c, const char *msg);

void
connection_broadcast_servers(Connection *c,
                             const void *data, size_t length);

void connection_broadcast_servers_except(Connection *c,
                                         const void *data, size_t length,
                                         struct uo_server *except);

void
connection_broadcast_divert(Connection *c,
                            enum protocol_version new_protocol,
                            const void *old_data, size_t old_length,
                            const void *new_data, size_t new_length);


/* server list */

void
connection_server_add(Connection *c, LinkedServer *ls);

void
connection_server_remove(Connection *c, LinkedServer *ls);

LinkedServer *
connection_server_new(Connection *c, int fd);

void
connection_server_dispose(Connection *c, LinkedServer *ls);

void
connection_server_zombify(Connection *c, LinkedServer *ls);

/* world */

void
connection_world_clear(Connection *c);


/* walk */

void connection_walk_server_removed(WalkState *state,
                                    LinkedServer *server);

void
connection_walk_request(LinkedServer *server,
                        const struct uo_packet_walk *p);

void connection_walk_cancel(Connection *c,
                            const struct uo_packet_walk_cancel *p);

void connection_walk_ack(Connection *c,
                         const struct uo_packet_walk_ack *p);

/* reconnect */

int
connection_client_connect(Connection *c,
                          const struct sockaddr *server_address,
                          size_t server_address_length,
                          uint32_t seed);

void
connection_client_disconnect(StatefulClient *client);

void connection_disconnect(Connection *c);

void connection_reconnect(Connection *c);

void
connection_reconnect_delayed(Connection *c);

/* attach */

Connection *find_attach_connection(Connection *c);

void attach_after_play_server(Connection *c,
                              LinkedServer *ls);

void
attach_send_world(LinkedServer *ls);

/* command */

void
connection_handle_command(LinkedServer *server, const char *command);

#endif
