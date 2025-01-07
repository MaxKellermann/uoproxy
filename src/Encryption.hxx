// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <cstddef>
#include <cstdint>

namespace UO {

class LoginEncryption {
	uint32_t key1, key2;
	uint32_t table1, table2;

public:
	bool Init(uint32_t seed, const void *data) noexcept;

	void Decrypt(const void *src, void *dest, size_t length) noexcept;
};

class Encryption {
	enum class State : uint8_t {
		NEW,
		SEEDED,
		DISABLED,
		LOGIN,
		GAME,
	} state = State::NEW;

	uint32_t seed;

	LoginEncryption login;

	void *buffer = nullptr;
	size_t buffer_size = 0;

public:
	/**
	 * Throws on error
	 *
	 * @return encrypted data (may be the original #data pointer if the
	 * connection is not encrypted)
	 */
	const void *FromClient(const void *data, size_t length);
};

} // namespace UO
