/*
	This file is part of nusaOS.

	nusaOS is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	nusaOS is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with nusaOS.  If not, see <https://www.gnu.org/licenses/>.

	Copyright (c) nusaOS 2026. All rights reserved.
*/

#pragma once

#include "kernel/kstd/types.h"

class BochsVGADevice;

/**
 * Pre-userspace resolution selection menu.
 *
 * Shown at boot before any services or userspace processes start. Renders a
 * dialog over the framebuffer and blocks until the user presses a number key.
 *
 * Resolutions are capped at VBE_MAX_WIDTH / VBE_MAX_HEIGHT (2000px).
 * The chosen resolution is applied to BochsVGADevice and cached statically
 * for other components to query if needed.
 *
 * Boot flow: ResolutionMenu::show() -> boot logo -> desktop
 */
class ResolutionMenu {
public:
	struct Resolution {
		uint16_t width;
		uint16_t height;
		const char* label;
	};

	/**
	 * Display the resolution menu and block until the user makes a selection.
	 * The chosen resolution is applied to the VGA device before returning.
	 *
	 * @param vga An initialised BochsVGADevice instance.
	 */
	static void show(BochsVGADevice* vga);

	/** Width of the resolution chosen by the user. */
	static uint16_t selected_width();

	/** Height of the resolution chosen by the user. */
	static uint16_t selected_height();

private:
	static const Resolution resolutions[];
	static const int num_resolutions;

	static uint16_t s_selected_width;
	static uint16_t s_selected_height;

	static void draw_background(BochsVGADevice* vga, uint32_t color);
	static void draw_char(BochsVGADevice* vga, int x, int y, char c, uint32_t fg, uint32_t bg);
	static void draw_string(BochsVGADevice* vga, int x, int y, const char* str, uint32_t fg, uint32_t bg);
	static void draw_menu(BochsVGADevice* vga, int highlight);
	static int  string_len(const char* s);

	// PS/2 keyboard polling via I/O ports, no IRQ handler needed.
	static uint8_t poll_scancode();
	static char    scancode_to_digit(uint8_t sc);
};