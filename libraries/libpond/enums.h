/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2022 Byteduck */

#pragma once

namespace Pond {
	enum WindowType {
		DEFAULT = 0,
		MENU = 1,
		// DESKTOP: selalu di layer paling bawah, tidak bisa di-move_to_front,
		// tidak bisa di-focus saat diklik (klik diteruskan ke ikon di atasnya)
		DESKTOP = 2,
		// PANEL: seperti sandbar — selalu di atas window biasa, tidak bisa
		// ditimpa oleh window DEFAULT saat move_to_front
		PANEL = 3
	};
}