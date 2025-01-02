// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "SocketBuffer.hxx"
#include "BufferedIO.hxx"
#include "Log.hxx"
#include "net/IPv4Address.hxx"
#include "net/SocketError.hxx"
#include "net/StaticSocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"

#include <algorithm>

#include <assert.h>
#include <unistd.h>
#include <errno.h>

SocketBuffer::SocketBuffer(EventLoop &event_loop, UniqueSocketDescriptor &&s, size_t input_max,
			   size_t output_max,
			   SocketBufferHandler &_handler)
	:event(event_loop, BIND_THIS_METHOD(OnSocketReady), s.Release()),
	 defer_send(event_loop, BIND_THIS_METHOD(OnDeferredSend)),
	 input(input_max),
	 output(output_max),
	 handler(_handler)
{
	event.ScheduleRead();
}

SocketBuffer::~SocketBuffer() noexcept
{
	event.Close();
}

void
SocketBuffer::Append(std::size_t length) noexcept
{
	output.Append(length);

	defer_send.Schedule();
}

bool
SocketBuffer::Send(std::span<const std::byte> src) noexcept
{
	auto w = Write();
	if (src.size() > w.size())
		return false;

	std::copy(src.begin(), src.end(), w.begin());
	Append(src.size());
	return true;
}

uint32_t
SocketBuffer::GetName() const noexcept
{
	const auto address = event.GetSocket().GetLocalAddress();
	if (address.GetFamily() == AF_INET)
		return IPv4Address::Cast(address).GetNumericAddressBE();
	else
		return 0;
}

uint16_t
SocketBuffer::GetPort() const noexcept
{
	const auto address = event.GetSocket().GetLocalAddress();
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

	ssize_t nbytes = handler.OnSocketData(r);
	if (nbytes == 0)
		return false;

	input.Consume(nbytes);
	return true;
}

bool
SocketBuffer::FlushOutput()
{
	ssize_t nbytes = write_from_buffer(event.GetSocket(), output);
	if (nbytes == -2)
		return true;

	if (nbytes < 0)
		throw MakeSocketError("Failed to send");

	return true;
}

inline void
SocketBuffer::OnDeferredSend() noexcept
try {
	FlushOutput();

	if (output.empty())
		event.CancelWrite();
	else
		event.ScheduleWrite();
} catch (...) {
	handler.OnSocketError(std::current_exception());
}

inline void
SocketBuffer::OnSocketReady(unsigned events) noexcept
try {
	if (events & event.WRITE) {
		FlushOutput();

		if (output.empty())
			event.CancelWrite();
	}

	if (events & event.READ) {
		ssize_t nbytes = read_to_buffer(event.GetSocket(), input);
		if (nbytes > 0) {
			if (!SubmitData())
				return;
		} else if (nbytes == 0) {
			handler.OnSocketDisconnect();
			return;
		} else if (nbytes == -1) {
			throw MakeSocketError("Failed to receive");
		}

		if (input.IsFull())
			event.CancelRead();
	}

	if (events & (event.HANGUP|event.ERROR)) {
		handler.OnSocketDisconnect();
	}
} catch (...) {
	handler.OnSocketError(std::current_exception());
}
