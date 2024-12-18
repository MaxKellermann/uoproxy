// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "SocketBuffer.hxx"
#include "BufferedIO.hxx"
#include "Flush.hxx"
#include "Log.hxx"
#include "util/DynamicFifoBuffer.hxx"

#include <event.h>

#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>

struct SocketBuffer final : PendingFlush {
    const int fd;

    struct event recv_event, send_event;

    DynamicFifoBuffer<std::byte> input, output;

    SocketBufferHandler &handler;

    SocketBuffer(int _fd, size_t input_max,
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

    ssize_t nbytes = handler.OnSocketData(r.data, r.size);
    if (nbytes == 0)
        return false;

    input.Consume(nbytes);
    return true;
}

bool
SocketBuffer::FlushOutput()
{
    ssize_t nbytes = write_from_buffer(fd, output);
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

    assert(fd == sb->fd);

    ssize_t nbytes = read_to_buffer(fd, sb->input, 65536);
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

    assert(fd == sb->fd);

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

WritableBuffer<void>
sock_buff_write(SocketBuffer *sb) noexcept
{
    return sb->output.Write().ToVoid();
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
    if (length > w.size)
        return false;

    memcpy(w.data, data, length);
    sock_buff_append(sb, length);
    return true;
}

uint32_t sock_buff_sockname(const SocketBuffer *sb)
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int ret = getsockname(sb->fd, (struct sockaddr *)&addr, &len);
    if (ret) {
        log_errno("getsockname()");
        return 0;
    }
    return addr.sin_addr.s_addr;
}

uint16_t sock_buff_port(const SocketBuffer *sb)
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int ret = getsockname(sb->fd, (struct sockaddr *)&addr, &len);
    if (ret) {
        log_errno("getsockname()");
        return 0;
    }
    return addr.sin_port;
}

/*
 * constructor and destructor
 *
 */

inline
SocketBuffer::SocketBuffer(int _fd, size_t input_max,
                           size_t output_max,
                           SocketBufferHandler &_handler)
    :fd(_fd),
     input(input_max),
     output(output_max),
     handler(_handler)
{
    event_set(&recv_event, fd, EV_READ|EV_PERSIST,
              sock_buff_recv_callback, this);
    event_set(&send_event, fd, EV_WRITE|EV_PERSIST,
              sock_buff_send_callback, this);

    event_add(&recv_event, nullptr);
}

SocketBuffer *
sock_buff_create(int fd, size_t input_max,
                 size_t output_max,
                 SocketBufferHandler &handler)
{
    return new SocketBuffer(fd, input_max, output_max, handler);
}

SocketBuffer::~SocketBuffer() noexcept
{
    assert(fd >= 0);

    event_del(&recv_event);
    event_del(&send_event);

    close(fd);
}

void sock_buff_dispose(SocketBuffer *sb) {
    delete sb;
}
