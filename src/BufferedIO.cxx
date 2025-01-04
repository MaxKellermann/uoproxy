// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#include "BufferedIO.hxx"
#include "net/SocketDescriptor.hxx"
#include "util/DynamicFifoBuffer.hxx"

#include <assert.h>
#include <errno.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

ssize_t
read_to_buffer(SocketDescriptor s, DynamicFifoBuffer<std::byte> &buffer)
{
	assert(s.IsDefined());

	auto w = buffer.Write();
	if (w.empty())
		return -2;

	ssize_t nbytes = s.ReadNoWait(w);
	if (nbytes > 0)
		buffer.Append(nbytes);

	return nbytes;
}

ssize_t
write_from_buffer(SocketDescriptor s, DynamicFifoBuffer<std::byte> &buffer)
{
	assert(s.IsDefined());

	auto r = buffer.Read();
	if (r.empty())
		return -2;

	ssize_t nbytes = s.WriteNoWait(r);
	if (nbytes < 0 && errno != EAGAIN)
		return -1;

	if (nbytes <= 0)
		return r.size();

	buffer.Consume(nbytes);
	return (ssize_t)r.size() - nbytes;
}
