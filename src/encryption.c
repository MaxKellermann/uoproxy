/*
 * uoproxy
 *
 * (c) 2005-2012 Max Kellermann <max@duempel.org>
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

#include "encryption.h"
#include "packets.h"
#include "log.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

enum encryption_state {
    STATE_NEW,
    STATE_SEEDED,
    STATE_DISABLED,
    STATE_LOGIN,
    STATE_GAME,
};

struct login_encryption {
    uint32_t key1, key2;
    uint32_t table1, table2;
};

struct encryption {
    enum encryption_state state;

    uint32_t seed;

    struct login_encryption login;

    void *buffer;
    size_t buffer_size;
};

struct login_key {
    const char *version;
    uint32_t key1, key2;
};

static const struct login_key login_keys[] = {
    { "7.0.18.0", 0x2C612CDD, 0xA328227F },
    { "7.0.17", 0x2C29E6ED, 0xA30EFE7F },
    { "7.0.16", 0x2C11A4FD, 0xA313527F },
    { "7.0.15", 0x2CDA670D, 0xA3723E7F },
    { "7.0.14", 0x2C822D1D, 0xA35DA27F },
    { "7.0.13", 0x2D4AF72D, 0xA3B71E7F },
    { "7.0.12", 0x2D32853D, 0xA38A127F },
    { "7.0.10.3", 0x2da36d5d, 0xa3c0a27f },
    { "7.0.10", 0x1f9c9575, 0x1bd26d6b },
    { "7.0.1.1", 0x2FABA7ED, 0xA2C17E7F },
    { "7.0.11", 0x2FABA7ED, 0xA2C17E7F },
    { "7.0.6.5", 0x2EC3ED9D, 0xA274227F },
    { "7.0.4", 0x2FABA7ED, 0xA2C17E7F },
    { "7.0.3", 0x2FABA7ED, 0xA2C17E7F },
    { "7.0.2", 0x2FABA7ED, 0xA2C17E7F },
    { "7.0.0", 0x2F93A5FD, 0xA2DD527F },
    { "6.0.14", 0x2C022D1D, 0xA31DA27F },
    { "6.0.13", 0x2DCAF72D, 0xA3F71E7F },
    { "6.0.12", 0x2DB2853D, 0xA3CA127F },
    { "6.0.11", 0x2D7B574D, 0xA3AD9E7F },
    { "6.0.10", 0x2D236D5D, 0xA380A27F },
    { "6.0.9", 0x2EEB076D, 0xA263BE7F },
    { "6.0.8", 0x2ED3257D, 0xA27F527F },
    { "6.0.7", 0x2E9BC78D, 0xA25BFE7F },
    { "6.0.6", 0x2E43ED9D, 0xA234227F },
    { "6.0.5", 0x2E0B97AD, 0xA210DE7F },
    { "6.0.4", 0x2FF385BD, 0xA2ED127F },
    { "6.0.3", 0x2FBBB7CD, 0xA2C95E7F },
    { "6.0.2", 0x2F63ADDD, 0xA2A5227F },
    { "6.0.1", 0x2F2BA7ED, 0xA2817E7F },
    { "6.0.0", 0x2f13a5fd, 0xa29d527f },
    { "5.0.9", 0x2F6B076D, 0xA2A3BE7F },
    { "5.0.8", 0x2F53257D, 0xA2BF527F },
    { "5.0.7", 0x10140441, 0xA29BFE7F },
    { "5.0.6", 0x2fc3ed9c, 0xa2f4227f },
    { "5.0.5", 0x2f8b97ac, 0xa2d0de7f },
    { "5.0.4", 0x2e7385bc, 0xa22d127f },
    { "5.0.3", 0x2e3bb7cc, 0xa2095e7f },
    { "5.0.2", 0x2ee3addc, 0xa265227f },
    { "5.0.1", 0x2eaba7ec, 0xa2417e7f },
    { "5.0.0", 0x2E93A5FC, 0xA25D527F },
    { "4.0.11", 0x2C7B574C, 0xA32D9E7F },
    { "4.0.10", 0x2C236D5C, 0xA300A27F },
    { "4.0.9", 0x2FEB076C, 0xA2E3BE7F },
    { "4.0.8", 0x2FD3257C, 0xA2FF527F },
    { "4.0.7", 0x2F9BC78D, 0xA2DBFE7F },
    { "4.0.6", 0x2F43ED9C, 0xA2B4227F },
    { "4.0.5", 0x2F0B97AC, 0xA290DE7F },
    { "4.0.4", 0x2EF385BC, 0xA26D127F },
    { "4.0.3", 0x2EBBB7CC, 0xA2495E7F },
    { "4.0.2", 0x2E63ADDC, 0xA225227F },
    { "4.0.1", 0x2E2BA7EC, 0xA2017E7F },
    { "4.0.0", 0x2E13A5FC, 0xA21D527F },
    { "3.0.8", 0x2C53257C, 0xA33F527F },
    { "3.0.7", 0x2C1BC78C, 0xA31BFE7F },
    { "3.0.6", 0x2CC3ED9C, 0xA374227F },
    { "3.0.5", 0x2C8B97AC, 0xA350DE7F },
    { NULL, 0, 0 }
};


struct encryption *
encryption_new(void)
{
    struct encryption *e = malloc(sizeof(*e));
    if (e == NULL)
        abort();

    e->state = STATE_NEW;
    e->buffer = NULL;
    e->buffer_size = 0;
    return e;
}

void
encryption_free(struct encryption *e)
{
    free(e);
}

static bool
account_login_valid(const struct uo_packet_account_login *p)
{
    return p->cmd == PCK_AccountLogin &&
        p->username[29] == 0x00 && p->password[29] == 0x00;
}

static void
login_decrypt(struct login_encryption *e, const void *src0,
              void *dest0, size_t length)
{
    const uint8_t *src = src0, *src_end = src + length;
    uint8_t *dest = dest0;

    while (src != src_end) {
        *dest++ = (*src++ ^ e->table1);
        uint32_t edx = e->table2;
        uint32_t esi = e->table1 << 31;
        uint32_t eax = e->table2 >> 1;
        eax |= esi;
        eax ^= e->key1 - 1;
        edx <<= 31;
        eax >>= 1;
        uint32_t ecx = e->table1 >> 1;
        eax |= esi;
        ecx |= edx;
        eax ^= e->key1;
        ecx ^= e->key2;
        e->table1 = ecx;
        e->table2 = eax;
    }
}

static bool
encryption_login_init(struct login_encryption *e, uint32_t seed,
                      const void *data)
{
    e->table1 = (((~seed) ^ 0x00001357) << 16) | ((seed ^ 0x0000aaaa) & 0x0000ffff);
    e->table2 = ((seed ^ 0x43210000) >> 16) | (((~seed) ^ 0xabcd0000) & 0xffff0000);

    for (const struct login_key *i = login_keys; i->version != NULL; ++i) {
        e->key1 = i->key1;
        e->key2 = i->key2;

        struct login_encryption tmp = *e;
        struct uo_packet_account_login decrypted;
        login_decrypt(&tmp, data, &decrypted, sizeof(decrypted));
        if (account_login_valid(&decrypted)) {
            log(2, "login encryption for client version %s\n", i->version);
            return true;
        }
    }

    return false;
}

const void *
encryption_from_client(struct encryption *e,
                       const void *data0, size_t length)
{
    assert(e != NULL);
    assert(data0 != NULL);
    assert(length > 0);

    if (e->state == STATE_DISABLED)
        return data0;

    const uint8_t *const data = data0, *p = data, *const end = p + length;

    if (e->state == STATE_NEW) {
        if (p + sizeof(e->seed) > end)
            /* need more data */
            return NULL;

        if (p[0] == 0xef) {
            /* client 6.0.5+ */
            const struct uo_packet_seed *packet_seed =
                (const struct uo_packet_seed *)p;
            if (length < sizeof(*packet_seed))
                /* need more data */
                return NULL;

            e->seed = ntohl(packet_seed->seed);
            p += sizeof(*packet_seed);
        } else {
            e->seed = ntohl(*(const uint32_t *)p);
            p += sizeof(e->seed);
        }

        e->state = STATE_SEEDED;

        if (p == end)
            return data;
    }

    if (e->state == STATE_SEEDED) {
        const struct uo_packet_account_login *account_login =
            (const struct uo_packet_account_login *)p;
        const struct uo_packet_game_login *game_login =
            (const struct uo_packet_game_login *)p;

        if (p + sizeof(*account_login) == end) {
            if (account_login_valid(account_login)) {
                /* unencrypted account login */
                e->state = STATE_DISABLED;
                return data;
            }

            if (!encryption_login_init(&e->login, e->seed, p)) {
                log(2, "login encryption failure\n");
                e->state = STATE_DISABLED;
                return data;
            }

            e->state = STATE_LOGIN;
        } else if (p + sizeof(*game_login) == end) {
            if (game_login->cmd == PCK_GameLogin &&
                game_login->auth_id == e->seed) {
                /* unencrypted game login */
                e->state = STATE_DISABLED;
                return data;
            }

            e->state = STATE_GAME;
        } else {
            /* unrecognized; assume it's not encrypted */
            log(2, "unrecognized encryption\n");
            e->state = STATE_DISABLED;
            return data;
        }
    }

    assert(e->state == STATE_LOGIN || e->state == STATE_GAME);

    if (length > e->buffer_size) {
        free(e->buffer);
        e->buffer_size = ((length - 1) | 0xfff) + 1;
        e->buffer = malloc(e->buffer_size);
        if (e->buffer == NULL)
            abort();
    }

    uint8_t *dest = e->buffer;
    if (p > data) {
        size_t l = p - data;
        memcpy(dest, data, l);
        dest += l;
    }

    if (e->state == STATE_LOGIN) {
        login_decrypt(&e->login, p, dest, end - p);
    } else {
    }

    /* XXX decrypt */
    return e->buffer;
}
