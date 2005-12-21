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

#include <sys/types.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "ioutil.h"
#include "client.h"
#include "sockbuff.h"
#include "buffer.h"
#include "compression.h"
#include "packets.h"
#include "dump.h"

struct uo_client {
    struct sock_buff *sock;
    int compression_enabled;
    struct uo_decompression decompression;
    struct buffer *decompressed_buffer;
};

int uo_client_create(const struct addrinfo *server_address,
                     uint32_t seed,
                     struct uo_client **clientp) {
    int sockfd, ret;
    struct uo_client *client;

    sockfd = socket(server_address->ai_family, SOCK_STREAM, 0);
    if (sockfd < 0)
        return -errno;

    ret = connect(sockfd, server_address->ai_addr,
                  server_address->ai_addrlen);
    if (ret < 0) {
        int save_errno = errno;
        close(sockfd);
        return -save_errno;
    }

    client = (struct uo_client*)calloc(1, sizeof(*client));
    if (client == NULL) {
        close(sockfd);
        return -ENOMEM;
    }

    ret = sock_buff_create(sockfd, 8192, 65536, &client->sock);
    if (ret < 0) {
        free(client);
        close(sockfd);
        return -ENOMEM;
    }

    uo_decompression_init(&client->decompression);
    client->decompressed_buffer = buffer_new(65536);
    if (client->decompressed_buffer == NULL) {
        uo_client_dispose(client);
        return -ENOMEM;
    }

    /* seed must be the first 4 bytes, and it must be flushed */
    uo_client_send(client, (unsigned char*)&seed, sizeof(seed));
    ret = sock_buff_flush(client->sock);
    if (ret < 0) {
        uo_client_dispose(client);
        return ret;
    }

    *clientp = client;

    return 0;
}

void uo_client_dispose(struct uo_client *client) {
    buffer_delete(client->decompressed_buffer);
    if (client->sock != NULL)
        sock_buff_dispose(client->sock);
    free(client);
}

int uo_client_alive(const struct uo_client *client) {
    return client->sock != NULL && sock_buff_alive(client->sock);
}

int uo_client_fileno(const struct uo_client *client) {
    return client->sock->fd;
}

void uo_client_pre_select(struct uo_client *client,
                          struct selectx *sx) {
    if (client->sock != NULL)
        sock_buff_pre_select(client->sock, sx);
}

int uo_client_post_select(struct uo_client *client,
                          struct selectx *sx) {
    if (client->sock == NULL)
        return 0;

    return sock_buff_post_select(client->sock, sx);
}

static unsigned char *peek_from_buffer(struct uo_client *client,
                                       struct buffer *buffer,
                                       size_t *lengthp) {
    unsigned char *p;
    size_t length, packet_length;

    p = buffer_peek(buffer, &length);
    if (p == NULL)
        return NULL;

#ifdef DUMP_CLIENT_PEEK
    printf("peek from server, length=%zu:\n", length);
    fhexdump(stdout, "  ", p, length);
    fflush(stdout);
#endif

    assert(length > 0);

    packet_length = get_packet_length(p, length);
    if (packet_length == PACKET_LENGTH_INVALID) {
        fprintf(stderr, "malformed packet from server:\n");
        fhexdump(stderr, "  ", p, length);
        fflush(stderr);
        sock_buff_dispose(client->sock);
        client->sock = NULL;
        return NULL;
    }

#ifdef DUMP_HEADERS
    printf("from server: 0x%02x length=%zu\n", p[0], packet_length);
#endif
    if (packet_length == 0 || packet_length > length)
        return NULL;
#ifdef DUMP_CLIENT_RECEIVE
    fhexdump(stdout, "  ", p, packet_length);
    fflush(stdout);
#endif

    *lengthp = packet_length;
    return p;
}

void *uo_client_peek(struct uo_client *client, size_t *lengthp) {
    if (client->sock == NULL)
        return NULL;

    if (client->compression_enabled) {
        unsigned char *p;
        size_t length;

        p = buffer_peek(client->sock->input, &length);
        if (p != NULL) {
            ssize_t nbytes;

            buffer_commit(client->decompressed_buffer),

            nbytes = uo_decompress(&client->decompression,
                                   buffer_tail(client->decompressed_buffer),
                                   buffer_free(client->decompressed_buffer),
                                   p, length);
            if (nbytes < 0) {
                fprintf(stderr, "decompression failed\n");
                sock_buff_dispose(client->sock);
                client->sock = NULL;
                return NULL;
            }

            buffer_shift(client->sock->input, length);
            buffer_expand(client->decompressed_buffer, (size_t)nbytes);
        }

        return peek_from_buffer(client, client->decompressed_buffer, lengthp);
    } else {
        return peek_from_buffer(client, client->sock->input, lengthp);
    }
}

void uo_client_shift(struct uo_client *client, size_t nbytes) {
    if (client->sock == NULL)
        return;

    buffer_shift(client->compression_enabled
                 ? client->decompressed_buffer
                 : client->sock->input,
                 nbytes);
}

void uo_client_send(struct uo_client *client,
                    const void *src, size_t length) {
    assert(length > 0);

    if (client->sock == NULL)
        return;

#ifdef DUMP_CLIENT_SEND
    printf("sending to packet to server, length=%zu:\n", length);
    fhexdump(stdout, "  ", src, length);
    fflush(stdout);
#endif

    if (*(const unsigned char*)src == PCK_GameLogin)
        client->compression_enabled = 1;

    buffer_append(client->sock->output, src, length);
}
