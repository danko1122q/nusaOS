/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2024 Byteduck */

#include "ICMPSocket.h"
#include "Router.h"
#include "../kstd/KLog.h"
#include "../api/ipv4.h"

kstd::vector<kstd::Weak<ICMPSocket>> ICMPSocket::s_sockets;
Mutex ICMPSocket::s_sockets_lock { "ICMPSocket::sockets" };

ICMPSocket::ICMPSocket(): IPSocket(Type::Raw, 0) {
	// ID unik dari lower 16-bit pointer socket ini
	m_id = (uint16_t) ((uintptr_t) this & 0xFFFF);
}

ICMPSocket::~ICMPSocket() {
	LOCK(s_sockets_lock);
	for (size_t i = 0; i < s_sockets.size(); i++) {
		if (s_sockets[i].lock().get() == this) {
			s_sockets.erase(i);
			break;
		}
	}
}

ResultRet<kstd::Arc<ICMPSocket>> ICMPSocket::make() {
	auto sock = kstd::Arc(new ICMPSocket());
	LOCK(s_sockets_lock);
	s_sockets.push_back(sock);
	return sock;
}

void ICMPSocket::deliver_packet(const IPv4Packet& packet) {
	LOCK(s_sockets_lock);
	for (size_t i = 0; i < s_sockets.size(); i++) {
		auto sock = s_sockets[i].lock();
		if (!sock) {
			// Socket sudah mati, bersihkan
			s_sockets.erase(i--);
			continue;
		}
		// Kirim ke semua ICMPSocket yang aktif
		sock->recv_packet((uint8_t*) &packet, packet.length.val());
	}
}

Result ICMPSocket::do_bind() {
	m_bound = true;
	return Result::Success;
}

Result ICMPSocket::do_connect() {
	LOCK(m_lock);
	if (!m_bound)
		TRYRES(do_bind());
	m_connection_state = Connected;
	return Result::Success;
}

ssize_t ICMPSocket::do_recv(RecvdPacket* pkt, SafePointer<uint8_t> buf, size_t len) {
	// Kembalikan full IPv4 packet ke userspace (termasuk 20-byte IP header)
	// sehingga userspace bisa skip header dan baca ICMP payload
	auto& ipv4 = pkt->header();
	size_t total_size = ipv4.length.val();
	size_t nread = min(len, total_size);
	buf.write((uint8_t*) &ipv4, nread);
	pkt->port = 0;
	return (ssize_t) nread;
}

ResultRet<size_t> ICMPSocket::do_send(SafePointer<uint8_t> buf, size_t len) {
	if (len < sizeof(ICMPHeader))
		return Result(set_error(EINVAL));

	auto route = Router::get_route(m_dest_addr, m_bound_addr, m_bound_device, false);
	if (!route.mac || !route.adapter)
		return Result(set_error(EHOSTUNREACH));

	const size_t packet_len = sizeof(IPv4Packet) + len;
	auto pkt = TRY(route.adapter->alloc_packet(packet_len));
	auto* ipv4 = route.adapter->setup_ipv4_packet(pkt, route.mac, m_dest_addr, ICMP, len, 0, 64);

	// Copy ICMP packet dari userspace (termasuk header + payload)
	buf.read((uint8_t*) ipv4->payload, len);

	// Hitung ulang checksum byte-by-byte untuk menghindari unaligned access pada packed struct
	auto* icmp_hdr = (ICMPHeader*) ipv4->payload;
	icmp_hdr->checksum = 0;
	uint32_t sum = 0;
	auto* bytes = (const uint8_t*) icmp_hdr;
	for (size_t i = 0; i + 1 < len; i += 2)
		sum += (uint16_t)(bytes[i] << 8 | bytes[i + 1]);
	if (len & 1)
		sum += (uint16_t)(bytes[len - 1] << 8);
	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);
	icmp_hdr->checksum = ~(uint16_t) sum;

	route.adapter->send_packet(pkt);
	if (pkt->buffer)
		route.adapter->release_packet(pkt);

	return len;
}

Result ICMPSocket::do_listen() {
	return Result(EOPNOTSUPP);
}

Result ICMPSocket::shutdown_reading() {
	return Result::Success;
}

Result ICMPSocket::shutdown_writing() {
	return Result::Success;
}