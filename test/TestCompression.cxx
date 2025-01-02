// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Compression.hxx"
#include "util/SpanCast.hxx"

#include <gtest/gtest.h>

#include <algorithm> // for std::shuffle()
#include <random>
#include <string>

using std::string_view_literals::operator""sv;

[[gnu::pure]]
static std::string
CompressToString(std::string_view src)
{
	std::byte compressed_buffer[4096];
	const std::size_t compressed_size = UO::Compress(compressed_buffer, AsBytes(src));
	const auto compressed = std::span(compressed_buffer).first(compressed_size);
	return std::string{ToStringView(compressed)};
}

static std::size_t
ChunkedDecompress(std::span<std::byte> dest, std::span<const std::byte> src,
		  std::size_t chunk_size)
{
	assert(chunk_size > 0);

	UO::Decompression decompression;
	std::size_t n = 0;

	while (!src.empty()) {
		auto i = src;
		if (i.size() > chunk_size)
			i = i.first(chunk_size);
		src = src.subspan(i.size());

		const std::size_t decompressed_size = decompression.Decompress(dest, i);
		n += decompressed_size;
		dest = dest.subspan(decompressed_size);
	}

	return n;
}

static void
TestChunkedDecompress(std::string_view raw, std::span<const std::byte> compressed)
{
	std::size_t step_size = 1;
	if (compressed.size() >= 64)
		step_size = 17;

	for (std::size_t chunk_size = 1; chunk_size < compressed.size(); chunk_size += step_size) {
		std::byte decompressed_buffer[4096];
		const std::size_t decompressed_size = ChunkedDecompress(std::span(decompressed_buffer).first(raw.size()),
									compressed, chunk_size);
		const auto decompressed = std::span(decompressed_buffer).first(decompressed_size);
		EXPECT_EQ(raw, ToStringView(decompressed));
	}
}

static void
TestCompressDecompress(std::string_view raw)
{
	std::byte compressed_buffer[4096];
	const std::size_t compressed_size = UO::Compress(compressed_buffer, AsBytes(raw));
	const auto compressed = std::span(compressed_buffer).first(compressed_size);
	EXPECT_FALSE(compressed.empty());

	TestChunkedDecompress(raw, compressed);
}

struct CompressionTestCase {
	std::string_view raw, compressed;
};

static constexpr CompressionTestCase compression_test_cases[] = {
	{""sv, "\xd0"sv},
	{"\x00"sv, "\x34"sv},
	{"hello world"sv, "\x40\xf2\xd4\xd5\xe2\xdb\xfc\x43\x6a\xe4\xd0"sv},
	{"\x00\x01\x02\x03\x04"sv, "\x3f\x13\x4e\xba"sv},
};

TEST(Compression, Compress)
{
	for (const auto &i : compression_test_cases) {
		EXPECT_EQ(CompressToString(i.raw), i.compressed);
	}
}

TEST(Compression, Decompress)
{
	for (const auto i : compression_test_cases) {
		for (std::size_t chunk_size = 1; chunk_size < i.compressed.size(); ++chunk_size) {
			std::byte decompressed_buffer[4096];
			const std::size_t decompressed_size = ChunkedDecompress(std::span(decompressed_buffer).first(i.raw.size()),
										AsBytes(i.compressed), chunk_size);
			const auto decompressed = std::span(decompressed_buffer).first(decompressed_size);
			EXPECT_EQ(i.raw, ToStringView(decompressed));
		}
	}
}

TEST(Compression, DecompressMultipleEmpty)
{
	TestChunkedDecompress(""sv, AsBytes("\xd0\xd0\xd0\xd0\xd0\xd0\xd0\xd0"sv));
}

TEST(Compression, DecompressInterleavedNull)
{
	TestChunkedDecompress("\0\0\0"sv, AsBytes("\xd0\x34\xd0\x34\xd0\x34"sv));
}

TEST(Compression, CompressDecompress)
{
	TestCompressDecompress(""sv);
	TestCompressDecompress("\0"sv);
	TestCompressDecompress("hello world"sv);
	TestCompressDecompress("\x00\x01\x02\x03\x04"sv);

	std::array<std::byte, 0x100> all_bytes;
	for (std::size_t i = 0; i < all_bytes.size(); ++i)
		all_bytes[i] = static_cast<std::byte>(i);

	TestCompressDecompress(ToStringView(all_bytes));

	std::random_device rd;
	std::mt19937 g(rd());
	std::shuffle(all_bytes.begin(), all_bytes.end(), g);
	TestCompressDecompress(ToStringView(all_bytes));
}

TEST(Compression, CompressDecompressAll)
{
	std::byte compressed_buffer[4096];
	std::size_t compressed_total_size = 0;

	for (const auto i : compression_test_cases) {
		const std::size_t compressed_size = UO::Compress(std::span{compressed_buffer}.subspan(compressed_total_size), AsBytes(i.raw));
		compressed_total_size += compressed_size;
	}

	const auto compressed = std::span(compressed_buffer).first(compressed_total_size);
	EXPECT_FALSE(compressed.empty());

	for (std::size_t chunk_size = 1; chunk_size < compressed.size(); ++chunk_size) {
		std::byte decompressed_buffer[4096];
		const std::size_t decompressed_size = ChunkedDecompress(decompressed_buffer,
									compressed, chunk_size);
		auto decompressed = std::span(decompressed_buffer).first(decompressed_size);

		for (const auto i : compression_test_cases) {
			ASSERT_GE(decompressed.size(), i.raw.size());
			EXPECT_EQ(i.raw, ToStringView(decompressed.first(i.raw.size())));
			decompressed = decompressed.subspan(i.raw.size());
		}

		EXPECT_TRUE(decompressed.empty());
	}
}
