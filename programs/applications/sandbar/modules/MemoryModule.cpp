/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2023 Byteduck */

#include "MemoryModule.h"

MemoryModule::MemoryModule() {
}

float MemoryModule::plot_value() {
	// FIX: Buka stream baru setiap kali baca — /proc files harus dibaca dari awal
	// setiap update. Stream yang disimpan di member variable stuck di EOF setelah
	// pembacaan pertama, menyebabkan get_info() return error/garbage selamanya.
	Duck::FileInputStream stream("/proc/meminfo");
	auto val = Sys::Mem::get_info(stream);
	if(val.has_value())
		return val.value().used_frac();
	return 0.0;
}