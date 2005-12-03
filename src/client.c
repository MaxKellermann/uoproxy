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

#include "netutil.h"
#include "ioutil.h"
#include "client.h"
#include "sockbuff.h"
#include "buffer.h"
#include "compression.h"
#include "packets.h"

struct uo_client {
    struct sock_buff *sock;
    int compression_enabled;
    struct uo_decompression decompression;
    struct buffer *decompressed_buffer;
};

int uo_client_create(uint32_t ip, uint16_t port, struct uo_client **clientp) {
    int sockfd, ret;
    struct uo_client *client;

    sockfd = socket_connect(ip, port);
    if (sockfd < 0)
        return -errno;

    client = (struct uo_client*)calloc(1, sizeof(*client));
    if (client == NULL) {
        close(sockfd);
        return -ENOMEM;
    }

    ret = sock_buff_create(sockfd, 4096, 65536, &client->sock);
    if (ret < 0) {
        free(client);
        close(sockfd);
        return -ENOMEM;
    }

    uo_decompression_init(&client->decompression);
    client->decompressed_buffer = buffer_new(65536);
    if (client->decompressed_buffer == NULL) {
        sock_buff_dispose(client->sock);
        free(client);
        close(sockfd);
        return -ENOMEM;
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

static unsigned char *receive_from_buffer(struct uo_client *client,
                                          struct buffer *buffer,
                                          unsigned char *dest,
                                          size_t *lengthp) {
    unsigned char *p;
    size_t length, packet_length;

    p = buffer_peek(buffer, &length);
    if (p == NULL)
        return NULL;

    packet_length = packet_lengths[p[0]];
    if (packet_length == 0) {
        if (length < 3)
            return NULL;

        packet_length = ntohs(*(uint16_t*)(p + 1));
        if (packet_length == 0) {
            fprintf(stderr, "malformed packet from server\n");
            sock_buff_dispose(client->sock);
            client->sock = NULL;
            return NULL;
        }
    }

    printf("from server: 0x%02x length=%zu\n", p[0], packet_length);
    if (packet_length > length)
        return NULL;

    if (packet_length > *lengthp) {
        fprintf(stderr, "buffer too small\n");
        sock_buff_dispose(client->sock);
        client->sock = NULL;
        return NULL;
    }

    memcpy(dest, p, packet_length);
    buffer_remove_head(buffer, packet_length);
    *lengthp = packet_length;

    return dest;
}

unsigned char *uo_client_receive(struct uo_client *client,
                                 unsigned char *dest, size_t *lengthp) {
    if (client->sock == NULL)
        return NULL;

    if (client->compression_enabled) {
        unsigned char *p;
        size_t length;

        p = buffer_peek(client->sock->input, &length);
        if (p != NULL) {
            ssize_t nbytes;

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

            buffer_remove_head(client->sock->input, length);
            buffer_expand(client->decompressed_buffer, (size_t)nbytes);

            printf("decompressed %zu bytes to %zd\n",
                   length, nbytes);
        }

        return receive_from_buffer(client, client->decompressed_buffer,
                                   dest, lengthp);
    } else {
        return receive_from_buffer(client, client->sock->input,
                                   dest, lengthp);
    }
}

void uo_client_send(struct uo_client *client,
                    const unsigned char *src, size_t length) {
    assert(length > 0);

    if (client->sock == NULL)
        return;

    if (src[0] == PCK_GameLogin)
        client->compression_enabled = 1;

    buffer_append(client->sock->output, src, length);
}
