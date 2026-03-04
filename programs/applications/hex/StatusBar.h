/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 danko1122q */

#pragma once

#include <libui/widget/Widget.h>
#include <string>

class StatusBar : public UI::Widget {
public:
	WIDGET_DEF(StatusBar);

	void set_info(size_t cursor, size_t file_size, bool modified, const std::string& path);

	Gfx::Dimensions preferred_size() override;

protected:
	void do_repaint(const UI::DrawContext& ctx) override;

private:
	StatusBar();

	size_t      m_cursor    = 0;
	size_t      m_file_size = 0;
	bool        m_modified  = false;
	std::string m_path;
	uint8_t     m_byte_val  = 0;
};