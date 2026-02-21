/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2024 Byteduck */

#pragma once

#include "IPSocket.h"
#include "ICMP.h"

/**
 * ICMPSocket: Raw ICMP socket untuk mengirim Echo Request dan menerima Echo Reply.
 * Digunakan oleh program ping di userspace melalui socket(AF_INET, SOCK_RAW, IPPROTO_ICMP).
 */
class ICMPSocket: public IPSocket, public kstd::ArcSelf<ICMPSocket> {
public:
	static ResultRet<kstd::Arc<ICMPSocket>> make();
	~ICMPSocket() override;

	// Dipanggil oleh NetworkManager saat menerima ICMP Echo Reply
	static void deliver_packet(const IPv4Packet& packet);

protected:
	ICMPSocket();

	Result do_bind() override;
	Result do_connect() override;
	ssize_t do_recv(RecvdPacket* pkt, SafePointer<uint8_t> buf, size_t len) override;
	ResultRet<size_t> do_send(SafePointer<uint8_t> buf, size_t len) override;
	Result do_listen() override;
	Result shutdown_reading() override;
	Result shutdown_writing() override;

private:
	static kstd::vector<kstd::Weak<ICMPSocket>> s_sockets;
	static Mutex s_sockets_lock;

	uint16_t m_id = 0; // ID unik per socket untuk filter reply
};