// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Encryption.hxx"
#include "PacketStructs.hxx"
#include "Log.hxx"
#include "uo/Command.hxx"

#include <cassert>

#include <stdlib.h>

namespace UO {

struct login_key {
    const char *version;
    uint32_t key1, key2;
};

static constexpr struct login_key login_keys[] = {
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
    { nullptr, 0, 0 }
};

static bool
account_login_valid(const struct uo_packet_account_login *p)
{
    return p->cmd == UO::Command::AccountLogin &&
        p->credentials.username[29] == 0x00 &&
        p->credentials.password[29] == 0x00;
}

inline bool
LoginEncryption::Init(uint32_t seed, const void *data) noexcept
{
    table1 = (((~seed) ^ 0x00001357) << 16) | ((seed ^ 0x0000aaaa) & 0x0000ffff);
    table2 = ((seed ^ 0x43210000) >> 16) | (((~seed) ^ 0xabcd0000) & 0xffff0000);

    for (const struct login_key *i = login_keys; i->version != nullptr; ++i) {
        key1 = i->key1;
        key2 = i->key2;

        auto tmp = *this;
        struct uo_packet_account_login decrypted;
        tmp.Decrypt(data, &decrypted, sizeof(decrypted));
        if (account_login_valid(&decrypted)) {
            LogFmt(2, "login encryption for client version {:?}\n", i->version);
            return true;
        }
    }

    return false;
}

inline void
LoginEncryption::Decrypt(const void *src0,
                         void *dest0, size_t length) noexcept
{
    const uint8_t *src = (const uint8_t *)src0, *src_end = src + length;
    uint8_t *dest = (uint8_t *)dest0;

    while (src != src_end) {
        *dest++ = (*src++ ^ table1);
        uint32_t edx = table2;
        uint32_t esi = table1 << 31;
        uint32_t eax = table2 >> 1;
        eax |= esi;
        eax ^= key1 - 1;
        edx <<= 31;
        eax >>= 1;
        uint32_t ecx = table1 >> 1;
        eax |= esi;
        ecx |= edx;
        eax ^= key1;
        ecx ^= key2;
        table1 = ecx;
        table2 = eax;
    }
}

const void *
Encryption::FromClient(const void *data0, size_t length) noexcept
{
    assert(data0 != nullptr);
    assert(length > 0);

    if (state == State::DISABLED)
        return data0;

    const uint8_t *const data = (const uint8_t *)data0, *p = data, *const end = p + length;

    if (state == State::NEW) {
        if (p + sizeof(seed) > end)
            /* need more data */
            return nullptr;

        if (p[0] == 0xef) {
            /* client 6.0.5+ */
            const struct uo_packet_seed *packet_seed =
                (const struct uo_packet_seed *)p;
            if (length < sizeof(*packet_seed))
                /* need more data */
                return nullptr;

            seed = packet_seed->seed;
            p += sizeof(*packet_seed);
        } else {
            seed = *(const PackedBE32 *)p;
            p += sizeof(seed);
        }

        state = State::SEEDED;

        if (p == end)
            return data;
    }

    if (state == State::SEEDED) {
        const struct uo_packet_account_login *account_login =
            (const struct uo_packet_account_login *)p;
        const struct uo_packet_game_login *game_login =
            (const struct uo_packet_game_login *)p;

        if (p + sizeof(*account_login) == end) {
            if (account_login_valid(account_login)) {
                /* unencrypted account login */
                state = State::DISABLED;
                return data;
            }

            if (!login.Init(seed, p)) {
                Log(2, "login encryption failure\n");
                state = State::DISABLED;
                return data;
            }

            state = State::LOGIN;
        } else if (p + sizeof(*game_login) == end) {
            if (game_login->cmd == UO::Command::GameLogin &&
                game_login->auth_id == seed) {
                /* unencrypted game login */
                state = State::DISABLED;
                return data;
            }

            state = State::GAME;
        } else {
            /* unrecognized; assume it's not encrypted */
            Log(2, "unrecognized encryption\n");
            state = State::DISABLED;
            return data;
        }
    }

    assert(state == State::LOGIN || state == State::GAME);

    if (length > buffer_size) {
        free(buffer);
        buffer_size = ((length - 1) | 0xfff) + 1;
        buffer = malloc(buffer_size);
        if (buffer == nullptr)
            abort();
    }

    uint8_t *dest = (uint8_t *)buffer;
    if (p > data) {
        size_t l = p - data;
        memcpy(dest, data, l);
        dest += l;
    }

    if (state == State::LOGIN) {
        login.Decrypt(p, dest, end - p);
    } else {
    }

    /* XXX decrypt */
    return buffer;
}

} // namespace UO
