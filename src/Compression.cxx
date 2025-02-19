// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Compression.hxx"
#include "net/SocketProtocolError.hxx"

#include <array>
#include <cassert>

namespace UO {

struct HuffmanTreeNode {
	int_least16_t left, right;
};

/**
 * Decompression Table
 * This is a static huffman tree that is walked down for decompression,
 * negative nodes are final values (and 0),
 * positive nodes point to other nodes
 *
 * if drawn this tree is sorted from up to down (layer by layer) and left to right.
 */
static constexpr std::array<HuffmanTreeNode, 0x100> huffman_tree{{
	/*   0 */ { 1, 2 },
	/*   1 */ { 3, 4 },
	/*   2 */ { 5, 0 },
	/*   3 */ { 6, 7 },
	/*   4 */ { 8, 9 },
	/*   5 */ { 10, 11 },
	/*   6 */ { 12, 13 },
	/*   7 */ { -256, 14 },
	/*   8 */ { 15, 16 },
	/*   9 */ { 17, 18 },
	/*  10 */ { 19, 20 },
	/*  11 */ { 21, 22 },
	/*  12 */ { -1, 23 },
	/*  13 */ { 24, 25 },
	/*  14 */ { 26, 27 },
	/*  15 */ { 28, 29 },
	/*  16 */ { 30, 31 },
	/*  17 */ { 32, 33 },
	/*  18 */ { 34, 35 },
	/*  19 */ { 36, 37 },
	/*  20 */ { 38, 39 },
	/*  21 */ { 40, -64 },
	/*  22 */ { 41, 42 },
	/*  23 */ { 43, 44 },
	/*  24 */ { -6, 45 },
	/*  25 */ { 46, 47 },
	/*  26 */ { 48, 49 },
	/*  27 */ { 50, 51 },
	/*  28 */ { -119, 52 },
	/*  29 */ { -32, 53 },
	/*  30 */ { 54, -14 },
	/*  31 */ { 55, -5 },
	/*  32 */ { 56, 57 },
	/*  33 */ { 58, 59 },
	/*  34 */ { 60, -2 },
	/*  35 */ { 61, 62 },
	/*  36 */ { 63, 64 },
	/*  37 */ { 65, 66 },
	/*  38 */ { 67, 68 },
	/*  39 */ { 69, 70 },
	/*  40 */ { 71, 72 },
	/*  41 */ { -51, 73 },
	/*  42 */ { 74, 75 },
	/*  43 */ { 76, 77 },
	/*  44 */ { -101, -111 },
	/*  45 */ { -4, -97 },
	/*  46 */ { 78, 79 },
	/*  47 */ { -110, 80 },
	/*  48 */ { 81, -116 },
	/*  49 */ { 82, 83 },
	/*  50 */ { 84, -255 },
	/*  51 */ { 85, 86 },
	/*  52 */ { 87, 88 },
	/*  53 */ { 89, 90 },
	/*  54 */ { -15, -10 },
	/*  55 */ { 91, 92 },
	/*  56 */ { -21, 93 },
	/*  57 */ { -117, 94 },
	/*  58 */ { 95, 96 },
	/*  59 */ { 97, 98 },
	/*  60 */ { 99, 100 },
	/*  61 */ { -114, 101 },
	/*  62 */ { -105, 102 },
	/*  63 */ { -26, 103 },
	/*  64 */ { 104, 105 },
	/*  65 */ { 106, 107 },
	/*  66 */ { 108, 109 },
	/*  67 */ { 110, 111 },
	/*  68 */ { 112, -3 },
	/*  69 */ { 113, -7 },
	/*  70 */ { 114, -131 },
	/*  71 */ { 115, -144 },
	/*  72 */ { 116, 117 },
	/*  73 */ { -20, 118 },
	/*  74 */ { 119, 120 },
	/*  75 */ { 121, 122 },
	/*  76 */ { 123, 124 },
	/*  77 */ { 125, 126 },
	/*  78 */ { 127, 128 },
	/*  79 */ { 129, -100 },
	/*  80 */ { 130, -8 },
	/*  81 */ { 131, 132 },
	/*  82 */ { 133, 134 },
	/*  83 */ { -120, 135 },
	/*  84 */ { 136, -31 },
	/*  85 */ { 137, 138 },
	/*  86 */ { -109, -234 },
	/*  87 */ { 139, 140 },
	/*  88 */ { 141, 142 },
	/*  89 */ { 143, 144 },
	/*  90 */ { -112, 145 },
	/*  91 */ { -19, 146 },
	/*  92 */ { 147, 148 },
	/*  93 */ { 149, -66 },
	/*  94 */ { 150, -145 },
	/*  95 */ { -13, -65 },
	/*  96 */ { 151, 152 },
	/*  97 */ { 153, 154 },
	/*  98 */ { -30, 155 },
	/*  99 */ { 156, 157 },
	/* 100 */ { -99, 158 },
	/* 101 */ { 159, 160 },
	/* 102 */ { 161, 162 },
	/* 103 */ { -23, 163 },
	/* 104 */ { -29, 164 },
	/* 105 */ { -11, 165 },
	/* 106 */ { 166, -115 },
	/* 107 */ { 167, 168 },
	/* 108 */ { 169, 170 },
	/* 109 */ { -16, 171 },
	/* 110 */ { -34, 172 },
	/* 111 */ { 173, -132 },
	/* 112 */ { 174, -108 },
	/* 113 */ { 175, -22 },
	/* 114 */ { 176, -9 },
	/* 115 */ { 177, -84 },
	/* 116 */ { -17, -37 },
	/* 117 */ { -28, 178 },
	/* 118 */ { 179, 180 },
	/* 119 */ { 181, 182 },
	/* 120 */ { 183, 184 },
	/* 121 */ { 185, 186 },
	/* 122 */ { 187, -104 },
	/* 123 */ { 188, -78 },
	/* 124 */ { 189, -61 },
	/* 125 */ { -79, -178 },
	/* 126 */ { -59, -134 },
	/* 127 */ { 190, -25 },
	/* 128 */ { -83, -18 },
	/* 129 */ { 191, -57 },
	/* 130 */ { -67, 192 },
	/* 131 */ { -98, 193 },
	/* 132 */ { -12, -68 },
	/* 133 */ { 194, 195 },
	/* 134 */ { -55, -128 },
	/* 135 */ { -24, -50 },
	/* 136 */ { -70, 196 },
	/* 137 */ { -94, -33 },
	/* 138 */ { 197, -129 },
	/* 139 */ { -74, 198 },
	/* 140 */ { -82, 199 },
	/* 141 */ { -56, -87 },
	/* 142 */ { -44, 200 },
	/* 143 */ { -248, 201 },
	/* 144 */ { -163, -81 },
	/* 145 */ { -52, -123 },
	/* 146 */ { 202, -113 },
	/* 147 */ { -48, -41 },
	/* 148 */ { -122, -40 },
	/* 149 */ { 203, -90 },
	/* 150 */ { -54, 204 },
	/* 151 */ { -86, -192 },
	/* 152 */ { 205, 206 },
	/* 153 */ { 207, -130 },
	/* 154 */ { -53, 208 },
	/* 155 */ { -133, -45 },
	/* 156 */ { 209, 210 },
	/* 157 */ { 211, -91 },
	/* 158 */ { 212, 213 },
	/* 159 */ { -106, -88 },
	/* 160 */ { 214, 215 },
	/* 161 */ { 216, 217 },
	/* 162 */ { 218, -49 },
	/* 163 */ { 219, 220 },
	/* 164 */ { 221, 222 },
	/* 165 */ { 223, 224 },
	/* 166 */ { 225, 226 },
	/* 167 */ { 227, -102 },
	/* 168 */ { -160, 228 },
	/* 169 */ { -46, 229 },
	/* 170 */ { -127, 230 },
	/* 171 */ { -103, 231 },
	/* 172 */ { 232, 233 },
	/* 173 */ { -60, 234 },
	/* 174 */ { 235, -76 },
	/* 175 */ { 236, -121 },
	/* 176 */ { 237, -73 },
	/* 177 */ { -149, 238 },
	/* 178 */ { 239, -107 },
	/* 179 */ { -35, 240 },
	/* 180 */ { -71, -27 },
	/* 181 */ { -69, 241 },
	/* 182 */ { -89, -77 },
	/* 183 */ { -62, -118 },
	/* 184 */ { -75, -85 },
	/* 185 */ { -72, -58 },
	/* 186 */ { -63, -80 },
	/* 187 */ { 242, -42 },
	/* 188 */ { -150, -157 },
	/* 189 */ { -139, -236 },
	/* 190 */ { -126, -243 },
	/* 191 */ { -142, -214 },
	/* 192 */ { -138, -206 },
	/* 193 */ { -240, -146 },
	/* 194 */ { -204, -147 },
	/* 195 */ { -152, -201 },
	/* 196 */ { -227, -207 },
	/* 197 */ { -154, -209 },
	/* 198 */ { -153, -254 },
	/* 199 */ { -176, -156 },
	/* 200 */ { -165, -210 },
	/* 201 */ { -172, -185 },
	/* 202 */ { -195, -170 },
	/* 203 */ { -232, -211 },
	/* 204 */ { -219, -239 },
	/* 205 */ { -200, -177 },
	/* 206 */ { -175, -212 },
	/* 207 */ { -244, -143 },
	/* 208 */ { -246, -171 },
	/* 209 */ { -203, -221 },
	/* 210 */ { -202, -181 },
	/* 211 */ { -173, -250 },
	/* 212 */ { -184, -164 },
	/* 213 */ { -193, -218 },
	/* 214 */ { -199, -220 },
	/* 215 */ { -190, -249 },
	/* 216 */ { -230, -217 },
	/* 217 */ { -169, -216 },
	/* 218 */ { -191, -197 },
	/* 219 */ { -47, 243 },
	/* 220 */ { 244, 245 },
	/* 221 */ { 246, 247 },
	/* 222 */ { -148, -159 },
	/* 223 */ { 248, 249 },
	/* 224 */ { -92, -93 },
	/* 225 */ { -96, -225 },
	/* 226 */ { -151, -95 },
	/* 227 */ { 250, 251 },
	/* 228 */ { -241, 252 },
	/* 229 */ { -161, -36 },
	/* 230 */ { 253, 254 },
	/* 231 */ { -135, -39 },
	/* 232 */ { -187, -124 },
	/* 233 */ { 255, -251 },
	/* 234 */ { -162, -238 },
	/* 235 */ { -242, -38 },
	/* 236 */ { -43, -125 },
	/* 237 */ { -215, -253 },
	/* 238 */ { -140, -208 },
	/* 239 */ { -137, -235 },
	/* 240 */ { -158, -237 },
	/* 241 */ { -136, -205 },
	/* 242 */ { -155, -141 },
	/* 243 */ { -228, -229 },
	/* 244 */ { -213, -168 },
	/* 245 */ { -224, -194 },
	/* 246 */ { -196, -226 },
	/* 247 */ { -183, -233 },
	/* 248 */ { -231, -167 },
	/* 249 */ { -174, -189 },
	/* 250 */ { -252, -166 },
	/* 251 */ { -198, -222 },
	/* 252 */ { -188, -179 },
	/* 253 */ { -223, -182 },
	/* 254 */ { -180, -186 },
	/* 255 */ { -245, -247 },
}};

std::size_t
UO::Decompression::Decompress(std::span<std::byte> dest,
			      std::span<const std::byte> src)
{
	size_t dest_index = 0;

	while (1) {
		if (bit >= 8) {
			if (src.empty())
				return dest_index;

			value = src.front();
			src = src.subspan(1);

			bit = 0;
			mask = std::byte{0x80};
		}

		if ((value & mask) != std::byte{})
			treepos = huffman_tree[treepos].left;
		else
			treepos = huffman_tree[treepos].right;

		mask >>= 1;
		bit++;

		if (treepos <= 0) {
			/* leaf */
			if (treepos == -256) {
				/* special flush character */
				bit = 8;    /* flush rest of byte */
				treepos = 0;    /* start on tree top again */
				continue;
			}
			if (dest_index >= dest.size())
				/* Buffer full */
				throw SocketBufferFullError{"Decompression buffer is full"};

			dest[dest_index++] = static_cast<std::byte>(-treepos); /* data is negative value */
			treepos = 0; /* start on tree top again */
		}
	}
}

struct EncodedHuffmanByte {
	uint_least8_t n_bits;
	uint_least16_t value;
};

static constexpr void
HuffmanTreeToBitTableAt(std::array<EncodedHuffmanByte, 0x101> &dest,
			const std::array<HuffmanTreeNode, 0x100> &src,
			std::size_t tree_position,
			uint_least8_t n_bits, uint_least16_t value) noexcept;

static constexpr void
HuffmanTreeToBitTableFollow(std::array<EncodedHuffmanByte, 0x101> &dest,
			    const std::array<HuffmanTreeNode, 0x100> &src,
			    int_least16_t tree_position,
			    uint_least8_t n_bits, uint_least16_t value) noexcept
{
	assert(n_bits > 0);

	if (tree_position <= 0) {
		assert(tree_position >= -0x100);

		dest[-tree_position] = {n_bits, value};
	} else {
		HuffmanTreeToBitTableAt(dest, src, static_cast<std::size_t>(tree_position),
						   n_bits, value);
	}
}

static constexpr void
HuffmanTreeToBitTableAt(std::array<EncodedHuffmanByte, 0x101> &dest,
			const std::array<HuffmanTreeNode, 0x100> &src,
			std::size_t tree_position,
			uint_least8_t n_bits, uint_least16_t value) noexcept
{
	assert(static_cast<std::size_t>(tree_position) < src.size());

	++n_bits;
	value <<= 1;

	HuffmanTreeToBitTableFollow(dest, src, src[tree_position].left, n_bits, value | 1);
	HuffmanTreeToBitTableFollow(dest, src, src[tree_position].right, n_bits, value);
}

static constexpr std::array<EncodedHuffmanByte, 0x101>
HuffmanTreeToBitTable(const std::array<HuffmanTreeNode, 0x100> &src) noexcept
{
	std::array<EncodedHuffmanByte, 0x101> dest;
	HuffmanTreeToBitTableAt(dest, src, 0, 0, 0);
	return dest;
}

/**
 * Compression Table
 */
static constexpr auto bit_table = HuffmanTreeToBitTable(huffman_tree);

struct Compression {
	uint_least32_t out_data = 0;
	uint_least8_t bit = 0;

	constexpr void FeedChar(std::size_t ch) noexcept;

	constexpr void OutputBits(std::span<std::byte> dest,
				  std::size_t *dest_indexp);

	constexpr void FlushOutput(std::span<std::byte> dest,
				   std::size_t *dest_indexp);
};

constexpr void
Compression::FeedChar(std::size_t ch) noexcept
{
	const unsigned num_bits = bit_table[ch].n_bits;

	bit += num_bits;
	assert(bit < 31);

	out_data = (out_data << num_bits) | bit_table[ch].value;
}

constexpr void
Compression::OutputBits(std::span<std::byte> dest,
			std::size_t *dest_indexp)
{
	while (bit >= 8) {
		if (*dest_indexp >= dest.size())
			throw SocketBufferFullError{"Compression buffer is full"};

		bit -= 8;
		dest[(*dest_indexp)++] = static_cast<std::byte>(out_data >> bit);
	}
}

constexpr void
Compression::FlushOutput(std::span<std::byte> dest,
			 std::size_t *dest_indexp)
{
	if (bit <= 0)
		return;

	if (*dest_indexp >= dest.size())
		throw SocketBufferFullError{"Compression buffer is full"};

	out_data <<= (8 - bit);
	dest[(*dest_indexp)++] = static_cast<std::byte>(out_data);
}

std::size_t
Compress(std::span<std::byte> dest, std::span<const std::byte> src)
{
	Compression co;
	size_t dest_index = 0;

	for (const auto src_char : src) {
		co.FeedChar(static_cast<std::size_t>(src_char));
		co.OutputBits(dest, &dest_index);
	}

	/* special flush character */
	co.FeedChar(256);

	co.OutputBits(dest, &dest_index);
	co.FlushOutput(dest, &dest_index);

	return (ssize_t)dest_index;
}

} // namespace UO
