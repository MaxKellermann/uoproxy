/*
 * uoproxy
 *
 * (c) 2005-2007 Max Kellermann <max@duempel.org>
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

#include "cversion.h"
#include "packets.h"

#include <stdlib.h>
#include <string.h>

void
client_version_free(struct client_version *cv)
{
    if (cv->packet != NULL)
        free(cv->packet);
}

int
client_version_set(struct client_version *cv,
                   const char *version)
{
    size_t length = strlen(version);

    cv->packet = malloc(sizeof(*cv->packet) + length);
    if (cv->packet == NULL)
        return -1;

    cv->packet->cmd = PCK_ClientVersion;
    cv->packet->length = htons(sizeof(*cv->packet) + length);
    memcpy(cv->packet->version, version, length + 1);
    return 1;
}
