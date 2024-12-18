// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "SocketBuffer.hxx"
#include "BufferedIO.hxx"
#include "Flush.hxx"
#include "Log.hxx"
#include "net/IPv4Address.hxx"
#include "net/StaticSocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/DynamicFifoBuffer.hxx"

#include <event.h>

#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>

struct SocketBuffer final : PendingFlush {
    const UniqueSocketDescriptor socket;

    struct event recv_event, send_event;

    DynamicFifoBuffer<std::byte> input, output;

    SocketBufferHandler &handler;

    SocketBuffer(UniqueSocketDescriptor &&s, size_t input_max,
                 size_t output_max,
                 SocketBufferHandler &_handler);
    ~SocketBuffer() noexcept;

    using PendingFlush::ScheduleFlush;

    /**
     * @return false on error or if nothing was consumed
     */
    bool SubmitData();

    /**
     * Try to flush the output buffer.  Note that this function will
     * not trigger the free() callback.
     *
     * @return true on success, false on i/o error (see errno)
     */
    bool FlushOutput();

protected:
    /* virtual methods from PendingFlush */
    void DoFlush() noexcept override;
};

inline bool
SocketBuffer::SubmitData()
{
    auto r = input.Read();
    if (r.empty())
        return true;

    const ScopeLockFlush lock_flush;

    ssize_t nbytes = handler.OnSocketData(r.data(), r.size());
    if (nbytes == 0)
        return false;

    input.Consume(nbytes);
    return true;
}

bool
SocketBuffer::FlushOutput()
{
    ssize_t nbytes = write_from_buffer(socket, output);
    if (nbytes == -2)
        return true;

    if (nbytes < 0)
        return false;

    return true;
}

void
SocketBuffer::DoFlush() noexcept
{
    if (!FlushOutput())
        return;

    if (output.empty())
        event_del(&send_event);
}


/*
 * libevent callback function
 *
 */

static void
sock_buff_recv_callback(int fd, short event, void *ctx)
{
    auto sb = (SocketBuffer *)ctx;

    (void)event;

    assert(fd == sb->socket.Get());

    ssize_t nbytes = read_to_buffer(SocketDescriptor{fd}, sb->input);
    if (nbytes > 0) {
        if (!sb->SubmitData())
            return;
    } else if (nbytes == 0) {
        sb->handler.OnSocketDisconnect(0);
        return;
    } else if (nbytes == -1) {
        sb->handler.OnSocketDisconnect(errno);
        return;
    }

    if (sb->input.IsFull())
        event_del(&sb->recv_event);
}

static void
sock_buff_send_callback(int fd, short event, void *ctx)
{
    auto sb = (SocketBuffer *)ctx;

    (void)fd;
    (void)event;

    assert(fd == sb->socket.Get());

    if (!sb->FlushOutput()) {
        sb->handler.OnSocketDisconnect(errno);
        return;
    }

    if (sb->output.empty())
        event_del(&sb->send_event);
}


/*
 * methods
 *
 */

std::span<std::byte>
sock_buff_write(SocketBuffer *sb) noexcept
{
    return sb->output.Write();
}

void
sock_buff_append(SocketBuffer *sb, size_t length)
{
    sb->output.Append(length);

    event_add(&sb->send_event, nullptr);
    sb->ScheduleFlush();
}

bool
sock_buff_send(SocketBuffer *sb, const void *data, size_t length)
{
    auto w = sock_buff_write(sb);
    if (length > w.size())
        return false;

    memcpy(w.data(), data, length);
    sock_buff_append(sb, length);
    return true;
}

uint32_t sock_buff_sockname(const SocketBuffer *sb)
{
    const auto address = sb->socket.GetLocalAddress();
    if (address.GetFamily() == AF_INET)
        return IPv4Address::Cast(address).GetNumericAddressBE();
    else
        return 0;
}

uint16_t sock_buff_port(const SocketBuffer *sb)
{
    const auto address = sb->socket.GetLocalAddress();
    if (address.GetFamily() == AF_INET)
        return IPv4Address::Cast(address).GetPortBE();
    else
        return 0;
}

/*
 * constructor and destructor
 *
 */

inline
SocketBuffer::SocketBuffer(UniqueSocketDescriptor &&s, size_t input_max,
                           size_t output_max,
                           SocketBufferHandler &_handler)
    :socket(std::move(s)),
     input(input_max),
     output(output_max),
     handler(_handler)
{
    event_set(&recv_event, socket.Get(), EV_READ|EV_PERSIST,
              sock_buff_recv_callback, this);
    event_set(&send_event, socket.Get(), EV_WRITE|EV_PERSIST,
              sock_buff_send_callback, this);

    event_add(&recv_event, nullptr);
}

SocketBuffer *
sock_buff_create(UniqueSocketDescriptor &&s, size_t input_max,
                 size_t output_max,
                 SocketBufferHandler &handler)
{
    return new SocketBuffer(std::move(s), input_max, output_max, handler);
}

SocketBuffer::~SocketBuffer() noexcept
{
    assert(socket.IsDefined());

    event_del(&recv_event);
    event_del(&send_event);
}

void sock_buff_dispose(SocketBuffer *sb) {
    delete sb;
}
