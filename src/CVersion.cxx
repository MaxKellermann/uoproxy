// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "CVersion.hxx"
#include "VerifyPacket.hxx"
#include "uo/Command.hxx"
#include "uo/MakePacket.hxx"

#include <string>

#include <string.h>

ClientVersion::~ClientVersion() noexcept = default;

static int
client_version_compare(const char *a, const char *b)
{
	char *a_endptr, *b_endptr;
	unsigned long a_int, b_int;

	assert(a != nullptr);
	assert(b != nullptr);

	while (1) {
		a_int = strtoul(a, &a_endptr, 10);
		b_int = strtoul(b, &b_endptr, 10);

		if (a_int < b_int)
			return -1;
		if (a_int > b_int)
			return 1;

		if (*a_endptr < *b_endptr)
			return -1;
		if (*a_endptr > *b_endptr)
			return 1;

		if (*a_endptr == 0)
			return 0;

		a = a_endptr + 1;
		b = b_endptr + 1;
	}
}

static ProtocolVersion
determine_protocol_version(const char *version)
{
	if (client_version_compare(version, "7") >= 0)
		return ProtocolVersion::V7;
	else if (client_version_compare(version, "6.0.14") >= 0)
		return ProtocolVersion::V6_0_14;
	else if (client_version_compare(version, "6.0.5") >= 0)
		return ProtocolVersion::V6_0_5;
	else if (client_version_compare(version, "6.0.1.7") >= 0)
		return ProtocolVersion::V6;
	else if (client_version_compare(version, "1") >= 0)
		return ProtocolVersion::V5;
	else
		return ProtocolVersion::UNKNOWN;
}

int
ClientVersion::Set(const struct uo_packet_client_version *_packet,
		   size_t length) noexcept
{
	if (!packet_verify_client_version(_packet, length))
		return 0;

	packet = {_packet, length};

	if (protocol == ProtocolVersion::UNKNOWN)
		protocol = determine_protocol_version(_packet->version);
	return 1;
}

void
ClientVersion::Set(std::string_view version) noexcept
{
	packet = UO::MakeClientVersion(version);

	// TODO eliminate temporary std::string
	protocol = determine_protocol_version(std::string{version}.c_str());
}

void
ClientVersion::Seed(const struct uo_packet_seed &_seed) noexcept
{
	assert(seed == nullptr);
	assert(_seed.cmd == UO::Command::Seed);

	seed = std::make_unique<struct uo_packet_seed>(_seed);

	/* this packet is only know to 6.0.5.0 clients, so we don't check
	   the packet contents here */
	if (_seed.client_major >= 7)
		protocol = ProtocolVersion::V7;
	else
		protocol = ProtocolVersion::V6_0_5;
}
