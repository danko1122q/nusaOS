/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2024 Byteduck */

#include "kernel/device/I8042.h"
#include "AC97Device.h"
#include "BochsVGADevice.h"
#include "kernel/device/MultibootVGADevice.h"
#include "ResolutionMenu.h"

extern multiboot_info mboot_header;

void Device::arch_init() {
	I8042::init();

	auto dev = AC97Device::detect();
	if (!dev.is_error())
		dev.value().leak_ref();

	BochsVGADevice* bochs_vga = BochsVGADevice::create();
	if (!bochs_vga) {
		// Fall back to the framebuffer provided by the bootloader.
		auto* mboot_vga = MultibootVGADevice::create(&mboot_header);
		if (!mboot_vga || mboot_vga->is_textmode())
			PANIC("MBOOT_TEXTMODE", "nusaOS doesn't support textmode.");
		// Multiboot VGA doesn't support the resolution menu; carry on.
		return;
	}

	// Show the boot-time resolution picker. Blocks until the user makes a
	// selection, then applies the chosen resolution to the VGA device.
	ResolutionMenu::show(bochs_vga);
}