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

#include "ResolutionMenu.h"
#include "kernel/arch/i386/device/BochsVGADevice.h"
#include "kernel/IO.h"

// font8x8_basic is defined in font8x8_basic.h, pulled in by VirtualTTY.cpp.
// Declared extern here to avoid a multiple-definition error at link time.
extern const char font8x8_basic[128][8];

// Colors — 0xAARRGGBB, alpha ignored by hardware
#define COLOR_BG       0xFF0A246A
#define COLOR_TITLE_BG 0xFF000080
#define COLOR_FG       0xFFFFFFFF
#define COLOR_SELECTED 0xFFFFFF00
#define COLOR_BORDER   0xFFD4D0C8

// PS/2 ports
#define KBD_DATA_PORT   0x60
#define KBD_STATUS_PORT 0x64
#define KBD_OUTPUT_FULL 0x01

const ResolutionMenu::Resolution ResolutionMenu::resolutions[] = {
	{  800,  600, "800 x 600   (SVGA)"    },
	{ 1024,  768, "1024 x 768  (XGA)"     },
	{ 1280,  720, "1280 x 720  (HD 720p)" },
	{ 1280, 1024, "1280 x 1024 (SXGA)"    },
	{ 1600,  900, "1600 x 900  (HD+)"     },
	{ 1920, 1080, "1920 x 1080 (Full HD)" },
};

const int ResolutionMenu::num_resolutions =
	sizeof(ResolutionMenu::resolutions) / sizeof(ResolutionMenu::resolutions[0]);

uint16_t ResolutionMenu::s_selected_width  = 1024;
uint16_t ResolutionMenu::s_selected_height = 768;

// No libc in kernel context, so roll our own strlen.
// noinline prevents the compiler from substituting a strlen builtin call.
__attribute__((noinline)) int ResolutionMenu::string_len(const char* s) {
	const char* p = s;
	while (*p) p++;
	return (int)(p - s);
}

// Render a single 8x8 glyph onto the framebuffer.
// In font8x8_basic each byte is one row and bit 0 (LSB) is the leftmost pixel.
void ResolutionMenu::draw_char(BochsVGADevice* vga, int x, int y, char c,
                               uint32_t fg, uint32_t bg)
{
	if (c < 32 || c > 126) c = '?';
	const uint8_t* glyph = (const uint8_t*) font8x8_basic[(unsigned char)c];
	for (int row = 0; row < 8; row++) {
		uint8_t bits = glyph[row];
		for (int col = 0; col < 8; col++) {
			uint32_t color = (bits & (1u << col)) ? fg : bg;
			vga->set_pixel(x + col, y + row, color);
		}
	}
}

void ResolutionMenu::draw_string(BochsVGADevice* vga, int x, int y,
                                 const char* str, uint32_t fg, uint32_t bg)
{
	while (*str) {
		draw_char(vga, x, y, *str, fg, bg);
		x += 8;
		str++;
	}
}

void ResolutionMenu::draw_background(BochsVGADevice* vga, uint32_t color) {
	vga->clear(color);
}

void ResolutionMenu::draw_menu(BochsVGADevice* vga, int /*highlight*/)
{
	const uint16_t W = (uint16_t) vga->get_display_width();
	const uint16_t H = (uint16_t) vga->get_display_height();

	draw_background(vga, COLOR_BG);

	const int DW = 480, DH = 280;
	const int DX = (W - DW) / 2;
	const int DY = (H - DH) / 2;
	const uint32_t dlg_bg = 0xFF2B2B5E;

	// Dialog background
	for (int row = DY; row < DY + DH; row++)
		for (int col = DX; col < DX + DW; col++)
			vga->set_pixel(col, row, dlg_bg);

	// 3px border
	for (int t = 0; t < 3; t++) {
		for (int col = DX - t; col < DX + DW + t; col++) {
			vga->set_pixel(col, DY - t,           COLOR_BORDER);
			vga->set_pixel(col, DY + DH - 1 + t,  COLOR_BORDER);
		}
		for (int row = DY - t; row < DY + DH + t; row++) {
			vga->set_pixel(DX - t,           row, COLOR_BORDER);
			vga->set_pixel(DX + DW - 1 + t,  row, COLOR_BORDER);
		}
	}

	// Title bar
	const int TB_H = 24;
	for (int row = DY; row < DY + TB_H; row++)
		for (int col = DX; col < DX + DW; col++)
			vga->set_pixel(col, row, COLOR_TITLE_BG);

	draw_string(vga, DX + 4,  DY + 8,          " nusaOS - Select Resolution",  COLOR_FG,  COLOR_TITLE_BG);
	draw_string(vga, DX + 16, DY + TB_H + 12,  "Press a number key to choose:", COLOR_FG,  dlg_bg);

	// Separator
	for (int col = DX + 8; col < DX + DW - 8; col++)
		vga->set_pixel(col, DY + TB_H + 26, COLOR_BORDER);

	// Resolution list
	int list_y = DY + TB_H + 36;
	for (int i = 0; i < num_resolutions; i++) {
		char num_buf[3] = { (char)('1' + i), '.', '\0' };
		draw_string(vga, DX + 24,      list_y, num_buf,              COLOR_SELECTED, dlg_bg);
		draw_string(vga, DX + 24 + 24, list_y, resolutions[i].label, COLOR_FG,       dlg_bg);
		list_y += 18;
	}

	// Footer
	for (int col = DX + 8; col < DX + DW - 8; col++)
		vga->set_pixel(col, DY + DH - 28, COLOR_BORDER);

	draw_string(vga, DX + 8, DY + DH - 20,
	            "Default: 1024x768 if no key is pressed",
	            0xFFAAAAAA, dlg_bg);
}

uint8_t ResolutionMenu::poll_scancode() {
	while (!(IO::inb(KBD_STATUS_PORT) & KBD_OUTPUT_FULL))
		;
	return IO::inb(KBD_DATA_PORT);
}

// Scancode set 1: digit keys 1–9 map to 0x02–0x0A.
char ResolutionMenu::scancode_to_digit(uint8_t sc) {
	if (sc >= 0x02 && sc <= 0x0A)
		return (char)('1' + (sc - 0x02));
	return 0;
}

// Discard any bytes already in the PS/2 output buffer before waiting for
// input — the controller may have leftover ACK bytes from keyboard init.
static void kbd_flush_buffer() {
	for (int i = 0; i < 32; i++) {
		if (!(IO::inb(0x64) & 0x01)) break;
		IO::inb(0x60);
		for (int d = 0; d < 10000; d++) { asm volatile("nop"); }
	}
}

void ResolutionMenu::show(BochsVGADevice* vga) {
	if (!vga) return;

	draw_menu(vga, -1);
	kbd_flush_buffer();

	int chosen = 0;

	while (true) {
		while (!(IO::inb(0x64) & 0x01))
			asm volatile("nop");

		uint8_t sc = IO::inb(0x60);

		if (sc & 0x80) continue;                 // key-release event
		if (sc == 0xFA || sc == 0x00) continue;  // controller ACK / null byte

		char d = scancode_to_digit(sc);
		if (d >= '1' && d <= '0' + num_resolutions) {
			chosen = d - '1';
			break;
		}
	}

	s_selected_width  = resolutions[chosen].width;
	s_selected_height = resolutions[chosen].height;

	vga->set_resolution(s_selected_width, s_selected_height);
	vga->clear(0x00000000);
}

uint16_t ResolutionMenu::selected_width()  { return s_selected_width;  }
uint16_t ResolutionMenu::selected_height() { return s_selected_height; }