// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "PVersion.hxx"
#include "util/VarStructPtr.hxx"

#include <cstddef>
#include <string_view>

struct ClientVersion {
	VarStructPtr<struct uo_packet_client_version> packet;
	struct uo_packet_seed *seed = nullptr;
	enum protocol_version protocol = PROTOCOL_UNKNOWN;

	ClientVersion() = default;
	~ClientVersion() noexcept;

	ClientVersion(const ClientVersion &) = delete;
	ClientVersion &operator=(const ClientVersion &) = delete;

	bool IsDefined() const noexcept {
		return packet;
	}

	int Set(const struct uo_packet_client_version *_packet,
		size_t length) noexcept;

	void Set(std::string_view version) noexcept;

	void Seed(const struct uo_packet_seed &_seed) noexcept;
};
