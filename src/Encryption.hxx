// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <stddef.h>

struct encryption;

struct encryption *
encryption_new();

void
encryption_free(struct encryption *e);

/**
 * @return encrypted data (may be the original #data pointer if the
 * connection is not encrypted), or nullptr if more data is necessary
 */
const void *
encryption_from_client(struct encryption *e,
                       const void *data, size_t length);
