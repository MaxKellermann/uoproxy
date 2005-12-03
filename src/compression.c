/*
 * uoproxy
 * (c) 2005 Max Kellermann <max@duempel.org>
 *
 * based on code from "Ultimate Melange"
 * Copyright (C) 2000 Axel Kittenberger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <assert.h>
#include <sys/types.h>

#include "compression.h"

/**
 * Decompression Table
 * This is a static huffman tree that is walked down for decompression,
 * negative nodes are final values (and 0),
 * positive nodes point to other nodes
 *
 * if drawn this tree is sorted from up to down (layer by layer) and left to right.
 */
static const int huffman_tree[] = {
    /*   0 */ 1, 2,
    /*   1 */ 3, 4,
    /*   2 */ 5, 0,
    /*   3 */ 6, 7,
    /*   4 */ 8, 9,
    /*   5 */ 10, 11,
    /*   6 */ 12, 13,
    /*   7 */ -256, 14,
    /*   8 */ 15, 16,
    /*   9 */ 17, 18,
    /*  10 */ 19, 20,
    /*  11 */ 21, 22,
    /*  12 */ -1, 23,
    /*  13 */ 24, 25,
    /*  14 */ 26, 27,
    /*  15 */ 28, 29,
    /*  16 */ 30, 31,
    /*  17 */ 32, 33,
    /*  18 */ 34, 35,
    /*  19 */ 36, 37,
    /*  20 */ 38, 39,
    /*  21 */ 40, -64,
    /*  22 */ 41, 42,
    /*  23 */ 43, 44,
    /*  24 */ -6, 45,
    /*  25 */ 46, 47,
    /*  26 */ 48, 49,
    /*  27 */ 50, 51,
    /*  28 */ -119, 52,
    /*  29 */ -32, 53,
    /*  30 */ 54, -14,
    /*  31 */ 55, -5,
    /*  32 */ 56, 57,
    /*  33 */ 58, 59,
    /*  34 */ 60, -2,
    /*  35 */ 61, 62,
    /*  36 */ 63, 64,
    /*  37 */ 65, 66,
    /*  38 */ 67, 68,
    /*  39 */ 69, 70,
    /*  40 */ 71, 72,
    /*  41 */ -51, 73,
    /*  42 */ 74, 75,
    /*  43 */ 76, 77,
    /*  44 */ -101, -111,
    /*  45 */ -4, -97,
    /*  46 */ 78, 79,
    /*  47 */ -110, 80,
    /*  48 */ 81, -116,
    /*  49 */ 82, 83,
    /*  50 */ 84, -255,
    /*  51 */ 85, 86,
    /*  52 */ 87, 88,
    /*  53 */ 89, 90,
    /*  54 */ -15, -10,
    /*  55 */ 91, 92,
    /*  56 */ -21, 93,
    /*  57 */ -117, 94,
    /*  58 */ 95, 96,
    /*  59 */ 97, 98,
    /*  60 */ 99, 100,
    /*  61 */ -114, 101,
    /*  62 */ -105, 102,
    /*  63 */ -26, 103,
    /*  64 */ 104, 105,
    /*  65 */ 106, 107,
    /*  66 */ 108, 109,
    /*  67 */ 110, 111,
    /*  68 */ 112, -3,
    /*  69 */ 113, -7,
    /*  70 */ 114, -131,
    /*  71 */ 115, -144,
    /*  72 */ 116, 117,
    /*  73 */ -20, 118,
    /*  74 */ 119, 120,
    /*  75 */ 121, 122,
    /*  76 */ 123, 124,
    /*  77 */ 125, 126,
    /*  78 */ 127, 128,
    /*  79 */ 129, -100,
    /*  80 */ 130, -8,
    /*  81 */ 131, 132,
    /*  82 */ 133, 134,
    /*  83 */ -120, 135,
    /*  84 */ 136, -31,
    /*  85 */ 137, 138,
    /*  86 */ -109, -234,
    /*  87 */ 139, 140,
    /*  88 */ 141, 142,
    /*  89 */ 143, 144,
    /*  90 */ -112, 145,
    /*  91 */ -19, 146,
    /*  92 */ 147, 148,
    /*  93 */ 149, -66,
    /*  94 */ 150, -145,
    /*  95 */ -13, -65,
    /*  96 */ 151, 152,
    /*  97 */ 153, 154,
    /*  98 */ -30, 155,
    /*  99 */ 156, 157,
    /* 100 */ -99, 158,
    /* 101 */ 159, 160,
    /* 102 */ 161, 162,
    /* 103 */ -23, 163,
    /* 104 */ -29, 164,
    /* 105 */ -11, 165,
    /* 106 */ 166, -115,
    /* 107 */ 167, 168,
    /* 108 */ 169, 170,
    /* 109 */ -16, 171,
    /* 110 */ -34, 172,
    /* 111 */ 173, -132,
    /* 112 */ 174, -108,
    /* 113 */ 175, -22,
    /* 114 */ 176, -9,
    /* 115 */ 177, -84,
    /* 116 */ -17, -37,
    /* 117 */ -28, 178,
    /* 118 */ 179, 180,
    /* 119 */ 181, 182,
    /* 120 */ 183, 184,
    /* 121 */ 185, 186,
    /* 122 */ 187, -104,
    /* 123 */ 188, -78,
    /* 124 */ 189, -61,
    /* 125 */ -79, -178,
    /* 126 */ -59, -134,
    /* 127 */ 190, -25,
    /* 128 */ -83, -18,
    /* 129 */ 191, -57,
    /* 130 */ -67, 192,
    /* 131 */ -98, 193,
    /* 132 */ -12, -68,
    /* 133 */ 194, 195,
    /* 134 */ -55, -128,
    /* 135 */ -24, -50,
    /* 136 */ -70, 196,
    /* 137 */ -94, -33,
    /* 138 */ 197, -129,
    /* 139 */ -74, 198,
    /* 140 */ -82, 199,
    /* 141 */ -56, -87,
    /* 142 */ -44, 200,
    /* 143 */ -248, 201,
    /* 144 */ -163, -81,
    /* 145 */ -52, -123,
    /* 146 */ 202, -113,
    /* 147 */ -48, -41,
    /* 148 */ -122, -40,
    /* 149 */ 203, -90,
    /* 150 */ -54, 204,
    /* 151 */ -86, -192,
    /* 152 */ 205, 206,
    /* 153 */ 207, -130,
    /* 154 */ -53, 208,
    /* 155 */ -133, -45,
    /* 156 */ 209, 210,
    /* 157 */ 211, -91,
    /* 158 */ 212, 213,
    /* 159 */ -106, -88,
    /* 160 */ 214, 215,
    /* 161 */ 216, 217,
    /* 162 */ 218, -49,
    /* 163 */ 219, 220,
    /* 164 */ 221, 222,
    /* 165 */ 223, 224,
    /* 166 */ 225, 226,
    /* 167 */ 227, -102,
    /* 168 */ -160, 228,
    /* 169 */ -46, 229,
    /* 170 */ -127, 230,
    /* 171 */ -103, 231,
    /* 172 */ 232, 233,
    /* 173 */ -60, 234,
    /* 174 */ 235, -76,
    /* 175 */ 236, -121,
    /* 176 */ 237, -73,
    /* 177 */ -149, 238,
    /* 178 */ 239, -107,
    /* 179 */ -35, 240,
    /* 180 */ -71, -27,
    /* 181 */ -69, 241,
    /* 182 */ -89, -77,
    /* 183 */ -62, -118,
    /* 184 */ -75, -85,
    /* 185 */ -72, -58,
    /* 186 */ -63, -80,
    /* 187 */ 242, -42,
    /* 188 */ -150, -157,
    /* 189 */ -139, -236,
    /* 190 */ -126, -243,
    /* 191 */ -142, -214,
    /* 192 */ -138, -206,
    /* 193 */ -240, -146,
    /* 194 */ -204, -147,
    /* 195 */ -152, -201,
    /* 196 */ -227, -207,
    /* 197 */ -154, -209,
    /* 198 */ -153, -254,
    /* 199 */ -176, -156,
    /* 200 */ -165, -210,
    /* 201 */ -172, -185,
    /* 202 */ -195, -170,
    /* 203 */ -232, -211,
    /* 204 */ -219, -239,
    /* 205 */ -200, -177,
    /* 206 */ -175, -212,
    /* 207 */ -244, -143,
    /* 208 */ -246, -171,
    /* 209 */ -203, -221,
    /* 210 */ -202, -181,
    /* 211 */ -173, -250,
    /* 212 */ -184, -164,
    /* 213 */ -193, -218,
    /* 214 */ -199, -220,
    /* 215 */ -190, -249,
    /* 216 */ -230, -217,
    /* 217 */ -169, -216,
    /* 218 */ -191, -197,
    /* 219 */ -47, 243,
    /* 220 */ 244, 245,
    /* 221 */ 246, 247,
    /* 222 */ -148, -159,
    /* 223 */ 248, 249,
    /* 224 */ -92, -93,
    /* 225 */ -96, -225,
    /* 226 */ -151, -95,
    /* 227 */ 250, 251,
    /* 228 */ -241, 252,
    /* 229 */ -161, -36,
    /* 230 */ 253, 254,
    /* 231 */ -135, -39,
    /* 232 */ -187, -124,
    /* 233 */ 255, -251,
    /* 234 */ -162, -238,
    /* 235 */ -242, -38,
    /* 236 */ -43, -125,
    /* 237 */ -215, -253,
    /* 238 */ -140, -208,
    /* 239 */ -137, -235,
    /* 240 */ -158, -237,
    /* 241 */ -136, -205,
    /* 242 */ -155, -141,
    /* 243 */ -228, -229,
    /* 244 */ -213, -168,
    /* 245 */ -224, -194,
    /* 246 */ -196, -226,
    /* 247 */ -183, -233,
    /* 248 */ -231, -167,
    /* 249 */ -174, -189,
    /* 250 */ -252, -166,
    /* 251 */ -198, -222,
    /* 252 */ -188, -179,
    /* 253 */ -223, -182,
    /* 254 */ -180, -186,
    /* 255 */ -245, -247,
};

void uo_decompression_init(struct uo_decompression *de) {
    de->bit = 8;
    de->treepos = 0;
    de->mask = 0;
    de->value = 0;
}

ssize_t uo_decompress(struct uo_decompression *de,
                      unsigned char *dest, size_t dest_max_len,
                      const unsigned char *src, size_t src_len) {
    size_t dest_index = 0;

    while (1) {
        if (de->bit >= 8) {
            if (src_len == 0)
                return (ssize_t)dest_index;

            de->value = *src++;
            src_len--;

            de->bit = 0;
            de->mask = 0x80;
        }

        if (de->value & de->mask)
            de->treepos = huffman_tree[de->treepos * 2];
        else
            de->treepos = huffman_tree[de->treepos * 2 + 1];

        de->mask >>= 1;
        de->bit++;

        if (de->treepos <= 0) {
            /* leaf */
            if (de->treepos == -256) {
                /* special flush character */
                de->bit = 8;    /* flush rest of byte */
                de->treepos = 0;    /* start on tree top again */
                continue;
            }
            if (dest_index >= dest_max_len)
                /* Buffer full */
                return -1;

            *dest++ = -de->treepos;   /* data is negative value */
            dest_index++;
            de->treepos = 0; /* start on tree top again */
        }
    }
}

/**
 * Compression Table
 *
 * This code was taken from Iris and is originally based on part of
 * UOX.
 */
static unsigned bit_table[257][2] =
{
    { 0x02, 0x00 }, { 0x05, 0x1F }, { 0x06, 0x22 }, { 0x07, 0x34 },
    { 0x07, 0x75 }, { 0x06, 0x28 }, { 0x06, 0x3B }, { 0x07, 0x32 },
    { 0x08, 0xE0 }, { 0x08, 0x62 }, { 0x07, 0x56 }, { 0x08, 0x79 },
    { 0x09, 0x19D }, { 0x08, 0x97 }, { 0x06, 0x2A }, { 0x07, 0x57 },
    { 0x08, 0x71 }, { 0x08, 0x5B }, { 0x09, 0x1CC }, { 0x08, 0xA7 },
    { 0x07, 0x25 }, { 0x07, 0x4F }, { 0x08, 0x66 }, { 0x08, 0x7D },
    { 0x09, 0x191 }, { 0x09, 0x1CE }, { 0x07, 0x3F }, { 0x09, 0x90 },
    { 0x08, 0x59 }, { 0x08, 0x7B }, { 0x08, 0x91 }, { 0x08, 0xC6 },
    { 0x06, 0x2D }, { 0x09, 0x186 }, { 0x08, 0x6F }, { 0x09, 0x93 },
    { 0x0A, 0x1CC }, { 0x08, 0x5A }, { 0x0A, 0x1AE }, { 0x0A, 0x1C0 },
    { 0x09, 0x148 }, { 0x09, 0x14A }, { 0x09, 0x82 }, { 0x0A, 0x19F },
    { 0x09, 0x171 }, { 0x09, 0x120 }, { 0x09, 0xE7 }, { 0x0A, 0x1F3 },
    { 0x09, 0x14B }, { 0x09, 0x100 }, { 0x09, 0x190 }, { 0x06, 0x13 },
    { 0x09, 0x161 }, { 0x09, 0x125 }, { 0x09, 0x133 }, { 0x09, 0x195 },
    { 0x09, 0x173 }, { 0x09, 0x1CA }, { 0x09, 0x86 }, { 0x09, 0x1E9 },
    { 0x09, 0xDB }, { 0x09, 0x1EC }, { 0x09, 0x8B }, { 0x09, 0x85 },
    { 0x05, 0x0A }, { 0x08, 0x96 }, { 0x08, 0x9C }, { 0x09, 0x1C3 },
    { 0x09, 0x19C }, { 0x09, 0x8F }, { 0x09, 0x18F }, { 0x09, 0x91 },
    { 0x09, 0x87 }, { 0x09, 0xC6 }, { 0x09, 0x177 }, { 0x09, 0x89 },
    { 0x09, 0xD6 }, { 0x09, 0x8C }, { 0x09, 0x1EE }, { 0x09, 0x1EB },
    { 0x09, 0x84 }, { 0x09, 0x164 }, { 0x09, 0x175 }, { 0x09, 0x1CD },
    { 0x08, 0x5E }, { 0x09, 0x88 }, { 0x09, 0x12B }, { 0x09, 0x172 },
    { 0x09, 0x10A }, { 0x09, 0x8D }, { 0x09, 0x13A }, { 0x09, 0x11C },
    { 0x0A, 0x1E1 }, { 0x0A, 0x1E0 }, { 0x09, 0x187 }, { 0x0A, 0x1DC },
    { 0x0A, 0x1DF }, { 0x07, 0x74 }, { 0x09, 0x19F }, { 0x08, 0x8D },
    { 0x08, 0xE4 }, { 0x07, 0x79 }, { 0x09, 0xEA }, { 0x09, 0xE1 },
    { 0x08, 0x40 }, { 0x07, 0x41 }, { 0x09, 0x10B }, { 0x09, 0xB0 },
    { 0x08, 0x6A }, { 0x08, 0xC1 }, { 0x07, 0x71 }, { 0x07, 0x78 },
    { 0x08, 0xB1 }, { 0x09, 0x14C }, { 0x07, 0x43 }, { 0x08, 0x76 },
    { 0x07, 0x66 }, { 0x07, 0x4D }, { 0x09, 0x8A }, { 0x06, 0x2F },
    { 0x08, 0xC9 }, { 0x09, 0xCE }, { 0x09, 0x149 }, { 0x09, 0x160 },
    { 0x0A, 0x1BA }, { 0x0A, 0x19E }, { 0x0A, 0x39F }, { 0x09, 0xE5 },
// 0x80:
    { 0x09, 0x194 }, { 0x09, 0x184 }, { 0x09, 0x126 }, { 0x07, 0x30 },
    { 0x08, 0x6C }, { 0x09, 0x121 }, { 0x09, 0x1E8 }, { 0x0A, 0x1C1 },
    { 0x0A, 0x11D }, { 0x0A, 0x163 }, { 0x0A, 0x385 }, { 0x0A, 0x3DB },
    { 0x0A, 0x17D }, { 0x0A, 0x106 }, { 0x0A, 0x397 }, { 0x0A, 0x24E },
    { 0x07, 0x2E }, { 0x08, 0x98 }, { 0x0A, 0x33C }, { 0x0A, 0x32E },
    { 0x0A, 0x1E9 }, { 0x09, 0xBF }, { 0x0A, 0x3DF }, { 0x0A, 0x1DD },
    { 0x0A, 0x32D }, { 0x0A, 0x2ED }, { 0x0A, 0x30B }, { 0x0A, 0x107 },
    { 0x0A, 0x2E8 }, { 0x0A, 0x3DE }, { 0x0A, 0x125 }, { 0x0A, 0x1E8 },
    { 0x09, 0xE9 }, { 0x0A, 0x1CD }, { 0x0A, 0x1B5 }, { 0x09, 0x165 },
    { 0x0A, 0x232 }, { 0x0A, 0x2E1 }, { 0x0B, 0x3AE }, { 0x0B, 0x3C6 },
    { 0x0B, 0x3E2 }, { 0x0A, 0x205 }, { 0x0A, 0x29A }, { 0x0A, 0x248 },
    { 0x0A, 0x2CD }, { 0x0A, 0x23B }, { 0x0B, 0x3C5 }, { 0x0A, 0x251 },
    { 0x0A, 0x2E9 }, { 0x0A, 0x252 }, { 0x09, 0x1EA }, { 0x0B, 0x3A0 },
    { 0x0B, 0x391 }, { 0x0A, 0x23C }, { 0x0B, 0x392 }, { 0x0B, 0x3D5 },
    { 0x0A, 0x233 }, { 0x0A, 0x2CC }, { 0x0B, 0x390 }, { 0x0A, 0x1BB },
    { 0x0B, 0x3A1 }, { 0x0B, 0x3C4 }, { 0x0A, 0x211 }, { 0x0A, 0x203 },
    { 0x09, 0x12A }, { 0x0A, 0x231 }, { 0x0B, 0x3E0 }, { 0x0A, 0x29B },
    { 0x0B, 0x3D7 }, { 0x0A, 0x202 }, { 0x0B, 0x3AD }, { 0x0A, 0x213 },
    { 0x0A, 0x253 }, { 0x0A, 0x32C }, { 0x0A, 0x23D }, { 0x0A, 0x23F },
    { 0x0A, 0x32F }, { 0x0A, 0x11C }, { 0x0A, 0x384 }, { 0x0A, 0x31C },
    { 0x0A, 0x17C }, { 0x0A, 0x30A }, { 0x0A, 0x2E0 }, { 0x0A, 0x276 },
    { 0x0A, 0x250 }, { 0x0B, 0x3E3 }, { 0x0A, 0x396 }, { 0x0A, 0x18F },
    { 0x0A, 0x204 }, { 0x0A, 0x206 }, { 0x0A, 0x230 }, { 0x0A, 0x265 },
    { 0x0A, 0x212 }, { 0x0A, 0x23E }, { 0x0B, 0x3AC }, { 0x0B, 0x393 },
    { 0x0B, 0x3E1 }, { 0x0A, 0x1DE }, { 0x0B, 0x3D6 }, { 0x0A, 0x31D },
    { 0x0B, 0x3E5 }, { 0x0B, 0x3E4 }, { 0x0A, 0x207 }, { 0x0B, 0x3C7 },
    { 0x0A, 0x277 }, { 0x0B, 0x3D4 }, { 0x08, 0xC0 }, { 0x0A, 0x162 },
    { 0x0A, 0x3DA }, { 0x0A, 0x124 }, { 0x0A, 0x1B4 }, { 0x0A, 0x264 },
    { 0x0A, 0x33D }, { 0x0A, 0x1D1 }, { 0x0A, 0x1AF }, { 0x0A, 0x39E },
    { 0x0A, 0x24F }, { 0x0B, 0x373 }, { 0x0A, 0x249 }, { 0x0B, 0x372 },
    { 0x09, 0x167 }, { 0x0A, 0x210 }, { 0x0A, 0x23A }, { 0x0A, 0x1B8 },
    { 0x0B, 0x3AF }, { 0x0A, 0x18E }, { 0x0A, 0x2EC }, { 0x07, 0x62 },
    { 0x04, 0x0D }
};

struct uo_compression {
    int bit;
    unsigned out_data;
};

static int output_bits(struct uo_compression *co,
                       unsigned char *dest,
                       size_t dest_max_len,
                       size_t *dest_indexp) {
    while (co->bit >= 8) {
        if (*dest_indexp >= dest_max_len)
            return -1;
        co->bit -= 8;
        dest[(*dest_indexp)++] = (co->out_data >> co->bit) & 0xff;
    }

    return 0;
}

ssize_t uo_compress(unsigned char *dest, size_t dest_max_len,
                    const unsigned char *src, size_t src_len) {
    struct uo_compression co = { .bit = 0, .out_data = 0 };
    size_t src_index, dest_index = 0;
    int num_bits;

    for (src_index = 0; src_index < src_len; src_index++) {
        const unsigned char src_char = src[src_index];

        num_bits = bit_table[src_char][0];
        co.bit += num_bits;
        assert(co.bit < 31);
        co.out_data = (co.out_data << num_bits) | bit_table[src_char][1];

        if (output_bits(&co, dest, dest_max_len, &dest_index) < 0)
            return -1;
    }

    num_bits = bit_table[256][0];
    co.bit += num_bits;
    assert(co.bit < 31);
    co.out_data = (co.out_data << num_bits) | bit_table[256][1];

    if (output_bits(&co, dest, dest_max_len, &dest_index) < 0)
        return -1;

    if (co.bit > 0) {
        if (dest_index >= dest_max_len)
            return -1;

        co.out_data <<= (8 - co.bit);
        dest[dest_index++] = co.out_data & 0xff;
    }

    return (ssize_t)dest_index;
}
