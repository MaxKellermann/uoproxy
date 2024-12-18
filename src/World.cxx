// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "World.hxx"
#include "Log.hxx"
#include "Bridge.hxx"
#include "PacketLengths.hxx"

#include <assert.h>
#include <string.h>

inline void
Item::Apply(const struct uo_packet_world_item &p) noexcept
{
    world_item_to_7(&socket.ground, &p);
}

Item *
World::FindItem(uint32_t serial) noexcept
{
    for (auto &i : items)
        if (i.serial == serial)
            return &i;

    return nullptr;
}

Item &
World::MakeItem(uint32_t serial) noexcept
{
    Item *i = FindItem(serial);
    if (i != nullptr)
        return *i;

    i = new Item(serial);
    items.push_front(*i);
    return *i;
}

void
World::Apply(const struct uo_packet_world_item &p) noexcept
{
    assert(p.cmd == UO::Command::WorldItem);
    assert(p.length <= sizeof(p));

    MakeItem(p.serial & PackedBE32(0x7fffffff)).Apply(p);
}

void
World::Apply(const struct uo_packet_world_item_7 &p) noexcept
{
    assert(p.cmd == UO::Command::WorldItem7);

    MakeItem(p.serial).Apply(p);
}

void
World::Apply(const struct uo_packet_equip &p) noexcept
{
    assert(p.cmd == UO::Command::Equip);

    MakeItem(p.serial).Apply(p);
}

void
World::Apply(const struct uo_packet_container_open &p) noexcept
{
    assert(p.cmd == UO::Command::ContainerOpen);

    MakeItem(p.serial).Apply(p);
}

void
World::SweepAfterContainerUpdate(uint32_t parent_serial) noexcept
{
    const unsigned attach_sequence = item_attach_sequence;

    items.remove_and_dispose_if([parent_serial, attach_sequence](const Item &i){
        return i.GetParentSerial() == parent_serial &&
            i.attach_sequence != attach_sequence;
    }, [](Item *i){ delete i; });
}

void
World::Apply(const struct uo_packet_container_open_7 &p) noexcept
{
    assert(p.base.cmd == UO::Command::ContainerOpen);

    Apply(p.base);
}

void
World::Apply(const struct uo_packet_container_update_6 &p) noexcept
{
    assert(p.cmd == UO::Command::ContainerUpdate);

    MakeItem(p.item.serial).Apply(p);
}

void
World::Apply(const struct uo_packet_container_content_6 &p) noexcept
{
    assert(p.cmd == UO::Command::ContainerContent);

    const unsigned attach_sequence = ++item_attach_sequence;

    const struct uo_packet_fragment_container_item_6 *pi = p.items,
        *const end = pi + p.num;

    for (; pi != end; ++pi) {
        auto &i = MakeItem(pi->serial);
        i.socket.container.cmd = UO::Command::ContainerUpdate;
        i.socket.container.item = *pi;
        i.attach_sequence = attach_sequence;
    }

    /* delete obsolete items; assuming that all parent_serials are the
       same, we use only the first one */
    if (p.num != 0)
        SweepAfterContainerUpdate(p.items[0].parent_serial);
}

void
World::RemoveItem(Item &item) noexcept
{
    item.unlink();
    delete &item;
}

void
World::RemoveItemTree(uint32_t parent_serial) noexcept
{
    IntrusiveList<Item> temp;

    /* move all direct children to the temporary list */
    items.remove_and_dispose_if([parent_serial](const Item &i){
        return i.GetParentSerial() == parent_serial;
    }, [&temp](Item *i){
        /* move to temp list */
        temp.push_back(*i);
    });

    /* delete these, and recursively delete their children */
    temp.clear_and_dispose([this](Item *i){
        RemoveItemTree(i->serial);
        delete i;
    });
}

void
World::RemoveItemSerial(uint32_t serial) noexcept
{
    /* remove this entity */
    Item *i = FindItem(serial);
    if (i != nullptr)
        RemoveItem(*i);

    /* remove equipped items */
    RemoveItemTree(serial);
}

Mobile *
World::FindMobile(uint32_t serial) noexcept
{
    for (auto &i : mobiles)
        if (i.serial == serial)
            return &i;

    return nullptr;
}

Mobile &
World::MakeMobile(uint32_t serial) noexcept
{
    Mobile *m = FindMobile(serial);
    if (m != nullptr)
        return *m;

    m = new Mobile(serial);
    mobiles.push_front(*m);
    return *m;
}

static void
read_equipped(World *world,
              const struct uo_packet_mobile_incoming *p)
{
    struct uo_packet_equip equip{};
    equip.cmd = UO::Command::Equip;
    equip.parent_serial = p->serial;

    const char *const p0 = (const char*)(const void*)p;
    const char *const end = p0 + p->length;
    const struct uo_packet_fragment_mobile_item *item = p->items;
    const char *i = (const char*)(const void*)item;

    while ((const char*)(const void*)(item + 1) <= end) {
        if (item->serial == 0)
            break;
        equip.serial = item->serial;
        equip.item_id = item->item_id & 0x3fff;
        equip.layer = item->layer;
        if (item->item_id & 0x8000) {
            equip.hue = item->hue;
            item++;
            i = (const char*)(const void*)item;
        } else {
            equip.hue = 0;
            i += sizeof(*item) - sizeof(item->hue);
            item = (const struct uo_packet_fragment_mobile_item*)i;
        }

        world->Apply(equip);
    }
}

void
World::Apply(const struct uo_packet_mobile_incoming &p) noexcept
{
    assert(p.cmd == UO::Command::MobileIncoming);

    if (p.serial == packet_start.serial) {
        /* update player's mobile */
        packet_start.body = p.body;
        packet_start.x = p.x;
        packet_start.y = p.y;
        packet_start.z = p.z;
        packet_start.direction = p.direction;

        packet_mobile_update.body = p.body;
        packet_mobile_update.hue = p.hue;
        packet_mobile_update.flags = p.flags;
        packet_mobile_update.x = p.x;
        packet_mobile_update.y = p.y;
        packet_mobile_update.direction = p.direction;
        packet_mobile_update.z = p.z;
    }

    auto &m = MakeMobile(p.serial);
    m.packet_mobile_incoming = {&p, p.length};

    read_equipped(this, &p);
}

void
World::Apply(const struct uo_packet_mobile_status &p) noexcept
{
    assert(p.cmd == UO::Command::MobileStatus);

    auto &m = MakeMobile(p.serial);

    /* XXX: check if p.flags is available */
    if (m.packet_mobile_status == nullptr ||
        m.packet_mobile_status->flags <= p.flags)
        m.packet_mobile_status = {&p, p.length};
}

void
World::Apply(const struct uo_packet_mobile_update &p) noexcept
{
    if (packet_start.serial == p.serial) {
        /* update player's mobile */
        packet_mobile_update = p;

        packet_start.body = p.body;
        packet_start.x = p.x;
        packet_start.y = p.y;
        packet_start.z = p.z;
        packet_start.direction = p.direction;
    }

    Mobile *m = FindMobile(p.serial);
    if (m == nullptr) {
        LogFmt(3, "warning in connection_mobile_update: no such mobile {:#x}\n",
               (uint32_t)p.serial);
        return;
    }

    /* copy values to m->packet_mobile_incoming */
    if (m->packet_mobile_incoming != nullptr) {
        m->packet_mobile_incoming->body = p.body;
        m->packet_mobile_incoming->x = p.x;
        m->packet_mobile_incoming->y = p.y;
        m->packet_mobile_incoming->z = p.z;
        m->packet_mobile_incoming->direction = p.direction;
        m->packet_mobile_incoming->hue = p.hue;
        m->packet_mobile_incoming->flags = p.flags;
    }
}

void
World::Apply(const struct uo_packet_mobile_moving &p) noexcept
{
    if (packet_start.serial == p.serial) {
        /* update player's mobile */
        packet_start.body = p.body;
        packet_start.x = p.x;
        packet_start.y = p.y;
        packet_start.z = p.z;
        packet_start.direction = p.direction;

        packet_mobile_update.body = p.body;
        packet_mobile_update.hue = p.hue;
        packet_mobile_update.flags = p.flags;
        packet_mobile_update.x = p.x;
        packet_mobile_update.y = p.y;
        packet_mobile_update.direction = p.direction;
        packet_mobile_update.z = p.z;
    }

    Mobile *m = FindMobile(p.serial);
    if (m == nullptr) {
        LogFmt(3, "warning in connection_mobile_moving: no such mobile {:#x}\n",
               (uint32_t)p.serial);
        return;
    }

    /* copy values to m->packet_mobile_incoming */
    if (m->packet_mobile_incoming != nullptr) {
        m->packet_mobile_incoming->body = p.body;
        m->packet_mobile_incoming->x = p.x;
        m->packet_mobile_incoming->y = p.y;
        m->packet_mobile_incoming->z = p.z;
        m->packet_mobile_incoming->direction = p.direction;
        m->packet_mobile_incoming->hue = p.hue;
        m->packet_mobile_incoming->flags = p.flags;
        m->packet_mobile_incoming->notoriety = p.notoriety;
    }
}

void
World::Apply(const struct uo_packet_zone_change &p) noexcept
{
    packet_start.x = p.x;
    packet_start.y = p.y;
    packet_start.z = p.z;

    packet_mobile_update.x = p.x;
    packet_mobile_update.y = p.y;
    packet_mobile_update.z = p.z;
}

void
World::RemoveMobile(Mobile &mobile) noexcept
{
    mobile.unlink();
    delete &mobile;
}

void
World::RemoveMobileSerial(uint32_t serial) noexcept
{
    /* remove this entity */
    Mobile *m = FindMobile(serial);
    if (m != nullptr)
        RemoveMobile(*m);

    /* remove equipped items */
    RemoveItemTree(serial);
}

void
World::RemoveSerial(uint32_t serial) noexcept
{
    if (serial < 0x40000000)
        RemoveMobileSerial(serial);
    else if (serial < 0x80000000)
        RemoveItemSerial(serial);
}

void
World::Walked(uint16_t x, uint16_t y,
              uint8_t direction, uint8_t notoriety) noexcept
{
    packet_start.x = x;
    packet_start.y = y;
    packet_start.direction = direction;

    packet_mobile_update.x = x;
    packet_mobile_update.y = y;
    packet_mobile_update.direction = direction;

    Mobile *m = FindMobile(packet_start.serial);
    if (m != nullptr && m->packet_mobile_incoming != nullptr) {
        m->packet_mobile_incoming->x = x;
        m->packet_mobile_incoming->y = y;
        m->packet_mobile_incoming->direction = direction;
        m->packet_mobile_incoming->notoriety = notoriety;
    }
}

void
World::WalkCancel(uint16_t x, uint16_t y, uint8_t direction) noexcept
{
    packet_start.x = x;
    packet_start.y = y;
    packet_start.direction = direction;

    packet_mobile_update.x = x;
    packet_mobile_update.y = y;
    packet_mobile_update.direction = direction;

    Mobile *m = FindMobile(packet_start.serial);
    if (m != nullptr && m->packet_mobile_incoming != nullptr) {
        m->packet_mobile_incoming->x = x;
        m->packet_mobile_incoming->y = y;
        m->packet_mobile_incoming->direction = direction;
    }
}
