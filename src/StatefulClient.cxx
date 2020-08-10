/*
 * uoproxy
 *
 * Copyright 2005-2020 Max Kellermann <max.kellermann@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "StatefulClient.hxx"
#include "Client.hxx"

#include <assert.h>

void
StatefulClient::Disconnect() noexcept
{
    assert(client != nullptr);

    if (reconnecting) {
        event_del(&reconnect_event);
        reconnecting = false;
    }

    version_requested = false;

    event_del(&ping_event);

    uo_client_dispose(client);
    client = nullptr;
}
