// SPDX-License-Identifier: GPL-2.0-only
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

class SocketAddress;
struct uo_packet_account_login;

void
SendUdpKnock(SocketAddress address,
	     const struct uo_packet_account_login &account_login);
