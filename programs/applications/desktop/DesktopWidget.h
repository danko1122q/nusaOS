/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2025-2026 danko1122q */

#pragma once

#include <libui/widget/Widget.h>
#include <libui/Timer.h>
#include <libapp/App.h>
#include <libpond/Event.h>
#include <vector>
#include <string>
#include <map>
#include <sys/stat.h>

#define DESKTOP_ICON_SIZE      32
#define DESKTOP_ICON_WIDTH     60
#define DESKTOP_ICON_HEIGHT    72
#define DESKTOP_ICON_PADDING   10
#define DESKTOP_LABEL_HEIGHT   30
#define DESKTOP_DRAG_THRESHOLD  4
#define DESKTOP_SANDBAR_HEIGHT 32
#define DESKTOP_PINS_FILE      "/etc/nusa_desktop_pins"
#define DESKTOP_PINS_POLL_MS   2000

struct DesktopIcon {
	App::Info  app;
	Gfx::Rect  rect;
	bool       hovered = false;
	bool       pressed = false;
};

class DesktopWidget: public UI::Widget {
public:
	WIDGET_DEF(DesktopWidget);

protected:
	void do_repaint(const UI::DrawContext& ctx) override;
	bool on_mouse_move(Pond::MouseMoveEvent evt) override;
	bool on_mouse_button(Pond::MouseButtonEvent evt) override;
	Gfx::Dimensions preferred_size() override;

private:
	DesktopWidget();

	void layout_icons();
	int  icon_at(Gfx::Point pos);
	void reload_icons();
	void save_pins();
	void remove_icon(int idx);

	std::vector<DesktopIcon> m_icons;
	int        m_last_hovered = -1;
	int        m_pressed_icon = -1;
	bool       m_dragging     = false;
	Gfx::Point m_drag_offset  = {0, 0};
	Gfx::Point m_press_pos    = {0, 0};
	bool       m_laid_out     = false;
	time_t     m_pins_mtime   = 0;
	bool       m_using_pins   = false;
	std::map<std::string, Gfx::Point> m_icon_positions;
	Duck::Ptr<UI::Timer> m_pins_timer;
};