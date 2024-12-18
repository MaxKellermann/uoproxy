// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <stddef.h>

struct sockaddr;

int
socket_connect(int domain, int type, int protocol,
               const struct sockaddr *address, size_t address_length);
