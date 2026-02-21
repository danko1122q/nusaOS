/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2024 Byteduck */

#include "DHCP.h"
#include <string.h>

DHCPPacket::DHCPPacket() {
	// Magic cookie
	m_packet.options[0] = 0x63;
	m_packet.options[1] = 0x82;
	m_packet.options[2] = 0x53;
	m_packet.options[3] = 0x63;
}

bool DHCPPacket::add_option(DHCPOption option, uint8_t size, const void* data) {
	if (m_options_offset + sizeof(uint8_t) * 2 + size > BOOTP_OPTS_MAXLEN)
		return false;
	m_packet.options[m_options_offset++] = option;
	m_packet.options[m_options_offset++] = size;
	if (data && size) {
		// FIX: memcpy ke &m_packet.options (base array) — selalu nulis ke offset 0!
		// Harusnya nulis ke &m_packet.options[m_options_offset] yaitu posisi saat ini.
		memcpy(&m_packet.options[m_options_offset], data, size);
		m_options_offset += size;
	}
	return true;
}

bool DHCPPacket::has_valid_cookie() const {
	return m_packet.options[0] == 0x63 && m_packet.options[1] == 0x82 && m_packet.options[2] == 0x53 && m_packet.options[3] == 0x63;
}

bool DHCPPacket::get_option(DHCPOption option, size_t size, void* ptr) const {
	size_t offset = 4; // skip magic cookie
	while (offset < BOOTP_OPTS_MAXLEN) {
		uint8_t opt_type = m_packet.options[offset];

		// FIX: cek opt_type == End (255) atau Pad (0) dulu sebelum akses offset+1
		// Kode asli cek options[offset] && options[offset+1] — ini salah karena:
		// 1. Pad option (0) valid tapi akan stop loop padahal seharusnya skip
		// 2. End option (255) tidak punya length byte, akses offset+1 bisa baca data salah
		if (opt_type == DHCPOption::End)
			break;
		if (opt_type == DHCPOption::Pad) {
			offset++;
			continue;
		}

		// Guard: pastikan offset+1 tidak keluar batas
		if (offset + 1 >= BOOTP_OPTS_MAXLEN)
			break;

		uint8_t opt_len = m_packet.options[offset + 1];

		// Guard: pastikan data option tidak keluar batas
		if (offset + 2 + opt_len > BOOTP_OPTS_MAXLEN)
			break;

		if (opt_type == option && opt_len == size) {
			memcpy(ptr, &m_packet.options[offset + 2], size);
			return true;
		}

		offset += 2 + opt_len;
	}
	return false;
}