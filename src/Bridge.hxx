// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

/*
 * Bridge between several protocol versions.  This code allows clients
 * implementing different protocol versions to coexist in multi-head
 * mode.
 */

#include "util/VarStructPtr.hxx"

#include <stddef.h>

struct uo_packet_container_update;
struct uo_packet_container_update_6;

void
container_update_5_to_6(struct uo_packet_container_update_6 *dest,
			const struct uo_packet_container_update *src);

void
container_update_6_to_5(struct uo_packet_container_update *dest,
			const struct uo_packet_container_update_6 *src);

struct uo_packet_container_content;
struct uo_packet_container_content_6;

VarStructPtr<struct uo_packet_container_content_6>
container_content_5_to_6(const struct uo_packet_container_content *src) noexcept;

VarStructPtr<struct uo_packet_container_content>
container_content_6_to_5(const struct uo_packet_container_content_6 *src) noexcept;

struct uo_packet_drop;
struct uo_packet_drop_6;

void
drop_5_to_6(struct uo_packet_drop_6 *dest,
	    const struct uo_packet_drop *src);

void
drop_6_to_5(struct uo_packet_drop *dest,
	    const struct uo_packet_drop_6 *src);

struct uo_packet_supported_features;
struct uo_packet_supported_features_6014;

void
supported_features_6_to_6014(struct uo_packet_supported_features_6014 *dest,
			     const struct uo_packet_supported_features *src);

void
supported_features_6014_to_6(struct uo_packet_supported_features *dest,
			     const struct uo_packet_supported_features_6014 *src);

struct uo_packet_world_item;
struct uo_packet_world_item_7;

void
world_item_to_7(struct uo_packet_world_item_7 *dest,
		const struct uo_packet_world_item *src);

void
world_item_from_7(struct uo_packet_world_item *dest,
		  const struct uo_packet_world_item_7 *src);
