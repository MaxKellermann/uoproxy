#include <sys/types.h>
#include <stdlib.h>

#include "handler.h"

packet_action_t handle_packet(struct packet_binding *bindings,
                              struct connection *c,
                              void *data, size_t length) {
    const unsigned char cmd
        = *(unsigned char*)data;

    for (; bindings->handler != NULL; bindings++) {
        if (bindings->cmd == cmd)
            return bindings->handler(c, data, length);
    }

    return PA_ACCEPT;
}
