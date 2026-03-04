/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 danko1122q */

#include "StatusBar.h"
#include <libui/libui.h>
#include <libui/Theme.h>
#include <cstdio>

using namespace UI;
using namespace Gfx;

static const Gfx::Color COL_BAR_BG   = {28,  28,  38};
static const Gfx::Color COL_BAR_FG   = {180, 180, 200};
static const Gfx::Color COL_MODIFIED = {220, 160, 60};
static const Gfx::Color COL_SEP      = {50,  50,  65};

StatusBar::StatusBar() {
	set_uses_alpha(false);
}

void StatusBar::set_info(size_t cursor, size_t file_size, bool modified, const std::string& path) {
	m_cursor    = cursor;
	m_file_size = file_size;
	m_modified  = modified;
	m_path      = path;
	repaint();
}

Gfx::Dimensions StatusBar::preferred_size() {
	return {100, 18};
}

void StatusBar::do_repaint(const DrawContext& ctx) {
	auto font = UI::Theme::font();
	Rect r    = ctx.rect();

	ctx.fill(r, COL_BAR_BG);
	ctx.fill({0, 0, r.width, 1}, COL_SEP);

	if(m_file_size == 0) {
		ctx.draw_text("No file loaded — use File > Open",
		              {4, 0, r.width - 4, r.height},
		              BEGINNING, CENTER, font, COL_BAR_FG);
		return;
	}

	// Left: file path + modified indicator
	char left_buf[256];
	snprintf(left_buf, sizeof(left_buf), "%s%s",
	         m_path.c_str(), m_modified ? " [modified]" : "");
	ctx.draw_text(left_buf, {4, 0, r.width / 2, r.height},
	              BEGINNING, CENTER, font, m_modified ? COL_MODIFIED : COL_BAR_FG);

	// Right: offset / size  |  hex value  |  dec value
	char right_buf[128];
	uint8_t byte_val = 0; // caller can extend to pass byte
	snprintf(right_buf, sizeof(right_buf),
	         "Offset: 0x%08zX  (%zu / %zu)",
	         m_cursor, m_cursor, m_file_size);
	ctx.draw_text(right_buf, {r.width / 2, 0, r.width / 2 - 4, r.height},
	              END, CENTER, font, COL_BAR_FG);
}