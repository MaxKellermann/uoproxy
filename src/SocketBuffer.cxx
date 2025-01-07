// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "SocketBuffer.hxx"
#include "Log.hxx"
#include "net/Buffered.hxx"
#include "net/IPv4Address.hxx"
#include "net/SocketError.hxx"
#include "net/StaticSocketAddress.hxx"
#include "net/UniqueSocketDescriptor.hxx"

#include <algorithm>

#include <assert.h>
#include <unistd.h>
#include <errno.h>

SocketBuffer::SocketBuffer(EventLoop &event_loop, UniqueSocketDescriptor &&s,
			   SocketBufferHandler &_handler)
	:socket(event_loop),
	 handler(_handler)
{
	socket.Init(s.Release(), FD_TCP, std::chrono::seconds{30}, *this);
	socket.ScheduleRead();
}

SocketBuffer::~SocketBuffer() noexcept
{
	socket.Close();
	socket.Destroy();
}

void
SocketBuffer::Append(std::size_t length) noexcept
{
	output.Append(length);

	socket.DeferWrite();
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

IPv4Address
SocketBuffer::GetLocalIPv4Address() const noexcept
{
	const auto address = socket.GetSocket().GetLocalAddress();
	if (address.GetFamily() == AF_INET)
		return IPv4Address::Cast(address);
	else
		return {};
}

BufferedResult
SocketBuffer::OnBufferedData() noexcept
{
	return handler.OnSocketData(socket.GetInputBuffer());
}

bool
SocketBuffer::OnBufferedClosed() noexcept
{
	return handler.OnSocketDisconnect();
}

bool
SocketBuffer::OnBufferedWrite()
{
	const auto r = output.Read();
	assert(!r.empty());

	const auto nbytes = socket.Write(r);
	assert(nbytes != 0);

	switch (nbytes) {
	case WRITE_ERRNO:
		throw MakeSocketError("Failed to send");

	case WRITE_BLOCKING:
	case WRITE_BROKEN:
		return true;

	case WRITE_DESTROYED:
		return false;
	}

	output.Consume(static_cast<std::size_t>(nbytes));

	if (output.empty()) {
		output.Free();
		socket.UnscheduleWrite();
	}

	return true;
}

void
SocketBuffer::OnBufferedError(std::exception_ptr e) noexcept
{
	handler.OnSocketError(std::move(e));
}
