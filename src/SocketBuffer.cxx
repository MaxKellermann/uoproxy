// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "SocketBuffer.hxx"
#include "BufferedIO.hxx"
#include "Log.hxx"
#include "net/IPv4Address.hxx"
#include "net/StaticSocketAddress.hxx"

#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

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

SocketBuffer::~SocketBuffer() noexcept = default;

void
SocketBuffer::Append(std::size_t length) noexcept
{
	output.Append(length);

	event.ScheduleWrite();
	ScheduleFlush();
}

bool
SocketBuffer::Send(const void *data, size_t length) noexcept
{
	auto w = Write();
	if (length > w.size())
		return false;

	memcpy(w.data(), data, length);
	Append(length);
	return true;
}

uint32_t
SocketBuffer::GetName() const noexcept
{
	const auto address = socket.GetLocalAddress();
	if (address.GetFamily() == AF_INET)
		return IPv4Address::Cast(address).GetNumericAddressBE();
	else
		return 0;
}

uint16_t
SocketBuffer::GetPort() const noexcept
{
	const auto address = socket.GetLocalAddress();
	if (address.GetFamily() == AF_INET)
		return IPv4Address::Cast(address).GetPortBE();
	else
		return 0;
}

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
