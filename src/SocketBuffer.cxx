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

#include "SocketBuffer.hxx"
#include "BufferedIO.hxx"
#include "FifoBuffer.hxx"
#include "Flush.hxx"
#include "Log.hxx"

#include <event.h>

#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>

struct SocketBuffer final : PendingFlush {
    const int fd;

    struct event recv_event, send_event;

    struct fifo_buffer *input = nullptr, *output = nullptr;

    SocketBufferHandler &handler;

    SocketBuffer(int _fd, size_t input_max,
                 size_t output_max,
                 SocketBufferHandler &_handler);
    ~SocketBuffer() noexcept;

    using PendingFlush::ScheduleFlush;

protected:
    /* virtual methods from PendingFlush */
    void DoFlush() noexcept override;
};


/*
 * invoke wrappers
 *
 */

/**
 * @return false on error or if nothing was consumed
 */
static bool
sock_buff_invoke_data(SocketBuffer *sb)
{
    const void *data;
    size_t length;
    ssize_t nbytes;

    data = fifo_buffer_read(sb->input, &length);
    if (data == nullptr)
        return true;

    const ScopeLockFlush lock_flush;

    nbytes = sb->handler.OnSocketData(data, length);
    if (nbytes == 0)
        return false;

    fifo_buffer_consume(sb->input, (size_t)nbytes);
    return true;
}

static void
sock_buff_invoke_free(SocketBuffer *sb, int error)
{
    assert(sb->fd >= 0);

    sb->handler.OnSocketDisconnect(error);
}


/*
 * flush
 *
 */

/**
 * Try to flush the output buffer.  Note that this function will not
 * trigger the free() callback.
 *
 * @return true on success, false on i/o error (see errno)
 */
static bool
sock_buff_flush(SocketBuffer *sb)
{
    ssize_t nbytes;

    nbytes = write_from_buffer(sb->fd, sb->output);
    if (nbytes == -2)
        return true;

    if (nbytes < 0)
        return false;

    return true;
}

void
SocketBuffer::DoFlush() noexcept
{
    if (!sock_buff_flush(this))
        return;

    if (fifo_buffer_empty(output))
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
        if (!sock_buff_invoke_data(sb))
            return;
    } else if (nbytes == 0) {
        sock_buff_invoke_free(sb, 0);
        return;
    } else if (nbytes == -1) {
        sock_buff_invoke_free(sb, errno);
        return;
    }

    if (fifo_buffer_full(sb->input))
        event_del(&sb->recv_event);
}

static void
sock_buff_send_callback(int fd, short event, void *ctx)
{
    auto sb = (SocketBuffer *)ctx;

    (void)fd;
    (void)event;

    assert(fd == sb->fd);

    if (!sock_buff_flush(sb)) {
        sock_buff_invoke_free(sb, errno);
        return;
    }

    if (fifo_buffer_empty(sb->output))
        event_del(&sb->send_event);
}


/*
 * methods
 *
 */

void *
sock_buff_write(SocketBuffer *sb, size_t *max_length_r)
{
    return fifo_buffer_write(sb->output, max_length_r);
}

void
sock_buff_append(SocketBuffer *sb, size_t length)
{
    fifo_buffer_append(sb->output, length);

    event_add(&sb->send_event, nullptr);
    sb->ScheduleFlush();
}

bool
sock_buff_send(SocketBuffer *sb, const void *data, size_t length)
{
    void *dest;
    size_t max_length;

    dest = sock_buff_write(sb, &max_length);
    if (dest == nullptr || length > max_length)
        return false;

    memcpy(dest, data, length);
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
     input(fifo_buffer_new(input_max)),
     output(fifo_buffer_new(output_max)),
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

    fifo_buffer_free(input);
    fifo_buffer_free(output);

}

void sock_buff_dispose(SocketBuffer *sb) {
    delete sb;
}
