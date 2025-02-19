// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

/*
 * Verify UO network packets.
 */

#include "uo/Packets.hxx"
#include "util/CharUtil.hxx"

#ifndef NDEBUG
#include "uo/Command.hxx"
#endif

#include <assert.h>

static inline bool
verify_printable_asciiz(const char *p, size_t length)
{
	size_t i;
	for (i = 0; i < length && p[i] != 0; ++i)
		if (!IsPrintableASCII(p[i]))
			return false;
	return true;
}

static inline bool
verify_printable_asciiz_n(const char *p, size_t length)
{
	size_t i;
	for (i = 0; i < length; ++i)
		if (!IsPrintableASCII(p[i]))
			return false;
	return p[length] == 0;
}

/**
 * Verifies that the specified packet really contains a string.
 */
static inline bool
packet_verify_client_version(const struct uo_packet_client_version *p,
			     size_t length)
{
	assert(length >= 3);
	assert(p->cmd == UO::Command::ClientVersion);

	return length > sizeof(*p) &&
		verify_printable_asciiz(p->version, length - sizeof(*p) + 1);
}

/**
 * Verifies that the packet length is correct for the number of items.
 */
static inline bool
packet_verify_container_content(const struct uo_packet_container_content *p,
				size_t length)
{
	assert(length >= 3);
	assert(p->cmd == UO::Command::ContainerContent);

	return length >= sizeof(*p) - sizeof(p->items) &&
		length == sizeof(*p) - sizeof(p->items) + p->num * sizeof(p->items[0]);
}

/**
 * Verifies that the packet length is correct for the number of items.
 */
static inline bool
packet_verify_container_content_6(const struct uo_packet_container_content_6 *p,
				  size_t length)
{
	assert(length >= 3);
	assert(p->cmd == UO::Command::ContainerContent);

	return length >= sizeof(*p) - sizeof(p->items) &&
		length == sizeof(*p) - sizeof(p->items) + p->num * sizeof(p->items[0]);
}
