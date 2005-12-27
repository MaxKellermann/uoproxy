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
#include <string.h>
#include <stdio.h>

#include "packets.h"
#include "connection.h"
#include "server.h"

static void walk_shift(struct connection_walk_state *state) {
    assert(state->queue_size > 0);

    --state->queue_size;

    if (state->queue_size == 0)
        state->server = NULL;
    else
        memmove(state->queue, state->queue + 1,
                state->queue_size * sizeof(*state->queue));
}

static const struct connection_walk_item *find_by_seq(struct connection_walk_state *state,
                                                      u_int8_t seq) {
    unsigned i;

    for (i = 0; i < state->queue_size; i++)
        if (state->queue[i].seq == seq)
            return &state->queue[i];

    return NULL;
}

static void remove_item(struct connection_walk_state *state,
                        const struct connection_walk_item *item) {
    unsigned i = item - state->queue;

    assert(i < state->queue_size);

    --state->queue_size;

    if (i < state->queue_size)
        memmove(state->queue + i, state->queue + 1,
                (state->queue_size - i) * sizeof(*state->queue));
    else if (state->queue_size == 0)
        state->server = NULL;
}

static void walk_clear(struct connection_walk_state *state) {
    state->server = NULL;
    state->queue_size = 0;
}

static void walk_cancel(struct connection *c,
                        struct uo_server *server,
                        const struct uo_packet_walk *old) {
    struct uo_packet_walk_cancel p = {
        .cmd = PCK_WalkCancel,
        .seq = old->seq,
        .x = c->packet_start.x,
        .y = c->packet_start.y,
        .direction = c->packet_start.direction,
        .z = c->packet_start.z,
    };

    uo_server_send(server, &p, sizeof(p));
}

void connection_walk_server_removed(struct connection_walk_state *state,
                                    struct linked_server *server) {
    if (state->server != server)
        return;

    state->server = NULL;
    state->queue_size = 0;
}

int connection_walk_request(struct connection *c,
                            struct linked_server *server,
                            struct uo_packet_walk *p) {
    struct connection_walk_state *state = &c->walk;
    struct connection_walk_item *i;

    assert(state->server != NULL || state->queue_size == 0);

    if (state->server != NULL && state->server != server) {
        printf("rejecting walk\n");
        walk_cancel(c, server->server, p);
        return 0;
    }

    if (state->queue_size >= MAX_WALK_QUEUE) {
        /* XXX */
        printf("queue full\n");
        walk_cancel(c, server->server, &state->queue[0].packet);
        walk_shift(state);
    }

    state->server = server;
    i = &state->queue[state->queue_size++];
    i->packet = *p;

#ifdef DUMP_WALK
    printf("walk seq_from_client=%u seq_to_server=%u\n", p->seq, state->seq_next);
#endif

    p->seq = i->seq = state->seq_next++;
    if (state->seq_next == 0)
        state->seq_next = 1;

    return 1;
}

void connection_walk_cancel(struct connection *c,
                            struct uo_packet_walk_cancel *p) {
    struct connection_walk_state *state = &c->walk;
    const struct connection_walk_item *i;

    state->seq_next = 0;

    if (state->server == NULL) {
        printf("WalkCancel out of sync II\n");
        return;
    }

    i = find_by_seq(state, p->seq);
    if (i == NULL) {
        printf("WalkCancel out of sync\n");
        return;
    }

#ifdef DUMP_WALK
    printf("walk_cancel seq_to_client=%u seq_from_server=%u\n", i->packet.seq, p->seq);
#endif

    /* only send to requesting client */
    if (!state->server->invalid) {
        p->seq = i->packet.seq;
        uo_server_send(state->server->server, p, sizeof(*p));
    }

    walk_clear(state);
}

void connection_walk_ack(struct connection *c,
                         struct uo_packet_walk_ack *p) {
    struct connection_walk_state *state = &c->walk;
    const struct connection_walk_item *i;
    unsigned x, y;

    if (state->server == NULL) {
        printf("WalkAck out of sync II\n");
        return;
    }

    i = find_by_seq(state, p->seq);
    if (i == NULL) {
        printf("WalkAck out of sync\n");
        return;
    }

#ifdef DUMP_WALK
    printf("walk_ack seq_to_client=%u seq_from_server=%u\n", i->packet.seq, p->seq);
#endif

    x = ntohs(c->packet_start.x);
    y = ntohs(c->packet_start.y);

    if ((c->packet_start.direction & 0x07) == (i->packet.direction & 0x07)) {
        switch (i->packet.direction & 0x07) {
        case 0: /* north */
            --y;
            break;
        case 1: /* north east */
            ++x;
            --y;
            break;
        case 2: /* east */
            ++x;
            break;
        case 3: /* south east */
            ++x;
            ++y;
            break;
        case 4: /* south */
            ++y;
            break;
        case 5: /* south west */
            --x;
            ++y;
            break;
        case 6: /* west */
            --x;
            break;
        case 7: /* north west */
            --x;
            --y;
            break;
        }
    }

    connection_walked(c, htons(x), htons(y),
                      i->packet.direction, p->notoriety);

    /* forward ack to requesting client */
    if (!state->server->invalid) {
        p->seq = i->packet.seq;
        uo_server_send(state->server->server, p, sizeof(*p));
    }

    /* send WalkForce to all other clients */
    if (c->servers_head != NULL && c->servers_head->next != NULL) {
        struct uo_packet_walk_force force = {
            .cmd = PCK_WalkForce,
            .direction = i->packet.direction & 0x7,
        };

        connection_broadcast_servers_except(c, &force, sizeof(force),
                                            state->server->server);
    }

    remove_item(state, i);
}
