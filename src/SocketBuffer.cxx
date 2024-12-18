// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "SocketBuffer.hxx"
#include "BufferedIO.hxx"
#include "Flush.hxx"
#include "Log.hxx"
#include "event/SocketEvent.hxx"
#include "net/IPv4Address.hxx"
#include "net/StaticSocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"
#include "util/DynamicFifoBuffer.hxx"

#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>

struct SocketBuffer final : PendingFlush {
    const UniqueSocketDescriptor socket;

    SocketEvent event;

    DynamicFifoBuffer<std::byte> input, output;

    SocketBufferHandler &handler;

    SocketBuffer(EventLoop &event_loop, UniqueSocketDescriptor &&s, size_t input_max,
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
    void OnSocketReady(unsigned events) noexcept;

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
        event.CancelWrite();
}

inline void
SocketBuffer::OnSocketReady(unsigned events) noexcept
{
    if (events & event.WRITE) {
        if (!FlushOutput()) {
            handler.OnSocketDisconnect(errno);
            return;
        }

        if (output.empty())
            event.CancelWrite();
    }

    if (events & event.READ) {
        ssize_t nbytes = read_to_buffer(socket, input);
        if (nbytes > 0) {
            if (!SubmitData())
                return;
        } else if (nbytes == 0) {
            handler.OnSocketDisconnect(0);
            return;
        } else if (nbytes == -1) {
            handler.OnSocketDisconnect(errno);
            return;
        }

        if (input.IsFull())
            event.CancelRead();
    }

    if (events & (event.HANGUP|event.ERROR)) {
        handler.OnSocketDisconnect(0);
    }
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

    sb->event.ScheduleWrite();
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
SocketBuffer::SocketBuffer(EventLoop &event_loop, UniqueSocketDescriptor &&s, size_t input_max,
                           size_t output_max,
                           SocketBufferHandler &_handler)
    :socket(std::move(s)),
     event(event_loop, BIND_THIS_METHOD(OnSocketReady), socket),
     input(input_max),
     output(output_max),
     handler(_handler)
{
    event.ScheduleRead();
}

SocketBuffer *
sock_buff_create(EventLoop &event_loop, UniqueSocketDescriptor &&s, size_t input_max,
                 size_t output_max,
                 SocketBufferHandler &handler)
{
    return new SocketBuffer(event_loop, std::move(s), input_max, output_max, handler);
}

inline
SocketBuffer::~SocketBuffer() noexcept = default;

void sock_buff_dispose(SocketBuffer *sb) {
    delete sb;
}
