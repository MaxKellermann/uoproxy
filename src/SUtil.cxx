// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Server.hxx"
#include "PacketStructs.hxx"
#include "uo/Command.hxx"
#include "util/VarStructPtr.hxx"

#include <string.h>

static void write_fixed_string(char *dest, size_t max_length,
                               const char *src) {
    size_t length = strlen(src);

    if (length >= max_length) {
        memcpy(dest, src, max_length);
    } else {
        memcpy(dest, src, length);
        memset(dest + length, 0, max_length - length);
    }
}

void
uo_server_speak_ascii(UO::Server *server,
                      uint32_t serial,
                      int16_t graphic,
                      uint8_t type,
                      uint16_t hue, uint16_t font,
                      const char *name,
                      const char *text)
{
    struct uo_packet_speak_ascii *p;
    const size_t text_length = strlen(text);

    VarStructPtr<struct uo_packet_speak_ascii> ptr(sizeof(*p) + text_length);
    p = ptr.get();

    p->cmd = UO::Command::SpeakAscii;
    p->length = ptr.size();
    p->serial = serial;
    p->graphic = graphic;
    p->type = type;
    p->hue = hue;
    p->font = font;
    write_fixed_string(p->name, sizeof(p->name), name);
    memcpy(p->text, text, text_length + 1);

    uo_server_send(server, ptr.get(), ptr.size());
}

void
uo_server_speak_console(UO::Server *server,
                        const char *text)
{
    uo_server_speak_ascii(server,
                          0xffffffff,
                          -1,
                          0x01,
                          0x35,
                          3,
                          "uoproxy", text);
}
