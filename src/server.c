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

#include <sys/types.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include "ioutil.h"
#include "server.h"
#include "sockbuff.h"
#include "buffer.h"
#include "compression.h"
#include "packets.h"
#include "dump.h"

struct uo_server {
    struct sock_buff *sock;
    uint32_t seed;
    int compression_enabled;
};

int uo_server_create(int sockfd, struct uo_server **serverp) {
    int ret;
    struct uo_server *server;

    server = (struct uo_server*)calloc(1, sizeof(*server));
    if (server == NULL)
        return -ENOMEM;

    ret = sock_buff_create(sockfd, 8192, 65536, &server->sock);
    if (ret < 0) {
        free(server);
        return -ENOMEM;
    }

    *serverp = server;

    return 0;
}

void uo_server_dispose(struct uo_server *server) {
    if (server->sock != NULL)
        sock_buff_dispose(server->sock);
    free(server);
}

int uo_server_alive(const struct uo_server *server) {
    return server->sock != NULL && sock_buff_alive(server->sock);
}

int uo_server_fileno(const struct uo_server *server) {
    return server->sock->fd;
}

u_int32_t uo_server_seed(const struct uo_server *server) {
    return server->seed;
}

void uo_server_pre_select(struct uo_server *server,
                          struct selectx *sx) {
    if (server->sock != NULL)
        sock_buff_pre_select(server->sock, sx);
}

int uo_server_post_select(struct uo_server *server,
                          struct selectx *sx) {
    if (server->sock == NULL)
        return 0;

    return sock_buff_post_select(server->sock, sx);
}

void *uo_server_peek(struct uo_server *server, size_t *lengthp) {
    unsigned char *p;
    size_t length, packet_length;

    if (server->sock == NULL)
        return NULL;

    p = buffer_peek(server->sock->input, &length);
    if (p == NULL)
        return NULL;

#ifdef DUMP_SERVER_PEEK
    printf("peek from client, length=%zu:\n", length);
    fhexdump(stdout, "  ", p, length);
    fflush(stdout);
#endif

    if (server->seed == 0) {
        /* the first packet from a client is the seed, 4 bytes without
           header */
        if (length < 4)
            return NULL;

        server->seed = *(uint32_t*)p;
        if (server->seed == 0) {
            fprintf(stderr, "zero seed from client\n");
            sock_buff_dispose(server->sock);
            server->sock = NULL;
            return NULL;
        }

        buffer_shift(server->sock->input, 4);

        p = buffer_peek(server->sock->input, &length);
        if (p == NULL)
            return NULL;
    }

    packet_length = get_packet_length(p, length);
    if (packet_length == PACKET_LENGTH_INVALID) {
        fprintf(stderr, "malformed packet from client:\n");
        fhexdump(stderr, "  ", p, length);
        fflush(stderr);
        sock_buff_dispose(server->sock);
        server->sock = NULL;
        return NULL;
    }

#ifdef DUMP_HEADERS
    printf("from client: 0x%02x length=%zu\n", p[0], packet_length);
#endif
    if (packet_length == 0 || packet_length > length)
        return NULL;
#ifdef DUMP_SERVER_RECEIVE
    fhexdump(stdout, "  ", p, packet_length);
    fflush(stdout);
#endif

    if (p[0] == PCK_GameLogin)
        server->compression_enabled = 1;

    *lengthp = packet_length;
    return p;
}

void uo_server_shift(struct uo_server *server, size_t nbytes) {
    if (server->sock == NULL)
        return;

    buffer_shift(server->sock->input, nbytes);
}

void uo_server_send(struct uo_server *server,
                    const void *src, size_t length) {
    assert(length > 0);
    assert(get_packet_length(src, length) == length);

    if (server->sock == NULL)
        return;

#ifdef DUMP_SERVER_SEND
    printf("sending to packet to client, length=%zu:\n", length);
    fhexdump(stdout, "  ", src, length);
    fflush(stdout);
#endif

    if (server->compression_enabled) {
        ssize_t nbytes;

        nbytes = uo_compress(buffer_tail(server->sock->output),
                             buffer_free(server->sock->output),
                             src, length);
        if (nbytes < 0) {
            fprintf(stderr, "uo_compress() failed\n");
            sock_buff_dispose(server->sock);
            server->sock = NULL;
            return;
        }

        buffer_expand(server->sock->output, (size_t)nbytes);
    } else {
        if (length > buffer_free(server->sock->output)) {
            fprintf(stderr, "output buffer full in uo_server_send()\n");
            sock_buff_dispose(server->sock);
            server->sock = NULL;
            return;
        }

        buffer_append(server->sock->output, src, length);
    }
}
