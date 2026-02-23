/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2023 Byteduck */

#include "CPUModule.h"
#include "../Sandbar.h"

CPUModule::CPUModule() {
}

float CPUModule::plot_value() {
	// FIX: Buka stream baru setiap kali baca — sama seperti MemoryModule,
	// stream member variable stuck di EOF setelah pembacaan pertama.
	Duck::FileInputStream stream("/proc/cpuinfo");
	auto val = Sys::CPU::get_info(stream);
	if(val.has_value())
		return val.value().utilization / 100.0;
	return 0.0;
}