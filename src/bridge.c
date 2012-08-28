/*
 * uoproxy
 *
 * (c) 2005-2010 Max Kellermann <max@duempel.org>
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

/*
 * Bridge between several protocol versions.  This code allows clients
 * implementing different protocol versions to coexist in multi-head
 * mode.
 */

#include "bridge.h"
#include "packets.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static void
container_item_5_to_6(struct uo_packet_fragment_container_item_6 *dest,
                      const struct uo_packet_fragment_container_item *src)
{
    dest->serial = src->serial;
    dest->item_id = src->item_id;
    dest->unknown0 = src->unknown0;
    dest->amount = src->amount;
    dest->x = src->x;
    dest->y = src->y;
    dest->unknown1 = 0;
    dest->parent_serial = src->parent_serial;
    dest->hue = src->hue;
}

/* container_update */

void
container_update_5_to_6(struct uo_packet_container_update_6 *dest,
                        const struct uo_packet_container_update *src)
{
    assert(src->cmd == PCK_ContainerUpdate);

    dest->cmd = PCK_ContainerUpdate;
    container_item_5_to_6(&dest->item, &src->item);
}

static void
container_item_6_to_5(struct uo_packet_fragment_container_item *dest,
                      const struct uo_packet_fragment_container_item_6 *src)
{
    dest->serial = src->serial;
    dest->item_id = src->item_id;
    dest->unknown0 = src->unknown0;
    dest->amount = src->amount;
    dest->x = src->x;
    dest->y = src->y;
    dest->parent_serial = src->parent_serial;
    dest->hue = src->hue;
}

void
container_update_6_to_5(struct uo_packet_container_update *dest,
                        const struct uo_packet_container_update_6 *src)
{
    assert(src->cmd == PCK_ContainerUpdate);

    dest->cmd = PCK_ContainerUpdate;
    container_item_6_to_5(&dest->item, &src->item);
}

/* container_content */

struct uo_packet_container_content_6 *
container_content_5_to_6(const struct uo_packet_container_content *src,
                         size_t *dest_length_r)
{
    size_t dest_length;
    struct uo_packet_container_content_6 *dest;
    unsigned num, i;

    assert(src->cmd == PCK_ContainerContent);

    num = ntohs(src->num);

    dest_length = sizeof(*dest) - sizeof(dest->items) +
        num * sizeof(dest->items);
    dest = malloc(dest_length);
    if (dest == NULL)
        return NULL;

    dest->cmd = PCK_ContainerContent;
    dest->length = htons(dest_length);
    dest->num = src->num;

    for (i = 0; i < num; ++i)
        container_item_5_to_6(&dest->items[i], &src->items[i]);

    *dest_length_r = dest_length;
    return dest;
}

struct uo_packet_container_content *
container_content_6_to_5(const struct uo_packet_container_content_6 *src,
                         size_t *dest_length_r)
{
    size_t dest_length;
    struct uo_packet_container_content *dest;
    unsigned num, i;

    assert(src->cmd == PCK_ContainerContent);

    num = ntohs(src->num);

    dest_length = sizeof(*dest) - sizeof(dest->items) +
        num * sizeof(dest->items);
    dest = malloc(dest_length);
    if (dest == NULL)
        return NULL;

    dest->cmd = PCK_ContainerContent;
    dest->length = htons(dest_length);
    dest->num = src->num;

    for (i = 0; i < num; ++i)
        container_item_6_to_5(&dest->items[i], &src->items[i]);

    *dest_length_r = dest_length;
    return dest;
}

/* drop */

void
drop_5_to_6(struct uo_packet_drop_6 *dest,
            const struct uo_packet_drop *src)
{
    assert(src->cmd == PCK_Drop);

    dest->cmd = PCK_Drop;
    dest->serial = src->serial;
    dest->x = src->x;
    dest->y = src->y;
    dest->z = src->z;
    dest->unknown0 = 0;
    dest->dest_serial = src->dest_serial;
}

void
drop_6_to_5(struct uo_packet_drop *dest,
            const struct uo_packet_drop_6 *src)
{
    assert(src->cmd == PCK_Drop);

    dest->cmd = PCK_Drop;
    dest->serial = src->serial;
    dest->x = src->x;
    dest->y = src->y;
    dest->z = src->z;
    dest->dest_serial = src->dest_serial;
}

void
supported_features_6_to_6014(struct uo_packet_supported_features_6014 *dest,
                             const struct uo_packet_supported_features *src)
{
    dest->cmd = PCK_SupportedFeatures;
    dest->flags = htonl(ntohs(src->flags));
}

void
supported_features_6014_to_6(struct uo_packet_supported_features *dest,
                             const struct uo_packet_supported_features_6014 *src)
{
    dest->cmd = PCK_SupportedFeatures;
    dest->flags = htons(ntohl(src->flags));
}

void
world_item_to_7(struct uo_packet_world_item_7 *dest,
                const struct uo_packet_world_item *src)
{
    memset(dest, 0, sizeof(*dest));

    bool have_amount = false;

    uint32_t serial = ntohl(src->serial);
    if (serial & 0x80000000) {
        serial &= ~0x80000000;
        have_amount = true;
    }

    const uint8_t *p = (const uint8_t *)&src->amount;

    if (have_amount) {
        dest->amount = dest->amount2 = *(const uint16_t *)p;
        p += 2;
    }

    uint16_t x = *(const uint16_t *)p;
    p += 2;
    uint16_t y = *(const uint16_t *)p;
    p += 2;

    const bool have_direction = (x & 0x8000) != 0;
    x &= ~0x8000;

    const bool have_hue = (y & 0x8000) != 0;
    const bool have_flags = (y & 0x4000) != 0;
    y &= ~0xc000;

    if (have_direction)
        dest->direction = *p++;

    dest->z = *p++;

    if (have_hue) {
        dest->hue = *(const uint16_t *)p;
        p += 2;
    }

    if (have_flags)
        dest->flags = *p++;

    dest->cmd = PCK_WorldItem7;
    dest->one = htons(0x0001);
    dest->type = 0x00;
    dest->serial = serial;
    dest->item_id = src->item_id;
}

void
world_item_from_7(struct uo_packet_world_item *dest,
                  const struct uo_packet_world_item_7 *src)
{
    dest->cmd = PCK_WorldItem;
    dest->serial = src->serial;
    dest->item_id = src->item_id;

    uint8_t *p = (uint8_t *)&dest->amount;

    if (src->amount != 0) {
        *(uint16_t *)p = src->amount;
        p += 2;

        dest->serial |= htonl(0x80000000);
    }

    uint16_t x = ntohs(src->x);
    if (src->direction != 0)
        x |= 0x8000;

    *(uint16_t *)p = htons(x);
    p += 2;

    uint16_t y = ntohs(src->y);
    if (src->hue != 0)
        y |= 0x8000;
    if (src->flags != 0)
        y |= 0x4000;

    *(uint16_t *)p = htons(y);
    p += 2;

    if (src->direction != 0)
        *p++ = src->direction;

    *p++ = src->z;

    if (src->hue != 0) {
        *(uint16_t *)p = src->hue;
        p += 2;
    }

    if (src->flags != 0)
        *p++ = src->flags;

    dest->length = htons(p - (const uint8_t *)dest);
}
