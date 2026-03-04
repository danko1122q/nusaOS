/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2025-2026 danko1122q */

#include "DesktopWidget.h"
#include <libui/libui.h>
#include <libui/DrawContext.h>
#include <libui/Theme.h>
#include <libui/Menu.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <vector>
#include <string>
#include <algorithm>

using namespace UI;
using namespace Gfx;

static std::vector<std::string> read_desktop_pins() {
	std::vector<std::string> pins;
	FILE* f = fopen(DESKTOP_PINS_FILE, "r");
	if(!f) return pins;
	char buf[256];
	while(fgets(buf, sizeof(buf), f)) {
		size_t len = strlen(buf);
		while(len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r'))
			buf[--len] = '\0';
		if(len > 0)
			pins.push_back(buf);
	}
	fclose(f);
	return pins;
}

static bool write_desktop_pins(const std::vector<std::string>& pins) {
	FILE* f = fopen(DESKTOP_PINS_FILE, "w");
	if(!f) return false;
	for(auto& p : pins)
		fprintf(f, "%s\n", p.c_str());
	fclose(f);
	return true;
}

static time_t get_pins_mtime() {
	struct stat st;
	if(stat(DESKTOP_PINS_FILE, &st) < 0) return 0;
	return st.st_mtime;
}

DesktopWidget::DesktopWidget() {
	reload_icons();

	m_pins_timer = UI::set_interval([&]() {
		time_t cur_mtime   = get_pins_mtime();
		bool   file_exists = (cur_mtime != 0);
		if(cur_mtime != m_pins_mtime || file_exists != m_using_pins)
			reload_icons();
	}, DESKTOP_PINS_POLL_MS);
}

void DesktopWidget::reload_icons() {
	// Save current positions so drag-placed icons survive a reload
	for(auto& icon : m_icons)
		if(icon.rect.width > 0)
			m_icon_positions[icon.app.name()] = icon.rect.position();

	m_pins_mtime   = get_pins_mtime();
	m_using_pins   = (m_pins_mtime != 0);
	m_icons.clear();
	m_laid_out     = false;
	m_last_hovered = -1;
	m_pressed_icon = -1;

	auto all_apps = App::get_all_apps();

	if(!m_using_pins) {
		for(auto& app : all_apps) {
			if(app.name().empty() || app.hidden()) continue;
			m_icons.push_back({app, {0, 0, 0, 0}});
		}
	} else {
		auto pins = read_desktop_pins();
		for(auto& pin_name : pins) {
			for(auto& app : all_apps) {
				if(app.name() == pin_name && !app.hidden()) {
					m_icons.push_back({app, {0, 0, 0, 0}});
					break;
				}
			}
		}
	}

	repaint();
}

void DesktopWidget::save_pins() {
	std::vector<std::string> pins;
	for(auto& icon : m_icons)
		pins.push_back(icon.app.name());
	write_desktop_pins(pins);
	m_pins_mtime = get_pins_mtime();
	m_using_pins = true;
}

void DesktopWidget::remove_icon(int idx) {
	if(idx < 0 || idx >= (int)m_icons.size()) return;
	if(!m_using_pins)
		save_pins();
	m_icons.erase(m_icons.begin() + idx);
	m_laid_out     = false;
	m_pressed_icon = -1;
	m_last_hovered = -1;
	save_pins();
	repaint();
}

void DesktopWidget::layout_icons() {
	if(m_laid_out) return;
	m_laid_out = true;

	auto dims = preferred_size();
	int x = DESKTOP_ICON_PADDING;
	int y = DESKTOP_ICON_PADDING;

	for(auto& icon : m_icons) {
		auto it = m_icon_positions.find(icon.app.name());
		if(it != m_icon_positions.end()) {
			icon.rect = {it->second.x, it->second.y, DESKTOP_ICON_WIDTH, DESKTOP_ICON_HEIGHT};
		} else {
			icon.rect = {x, y, DESKTOP_ICON_WIDTH, DESKTOP_ICON_HEIGHT};
			y += DESKTOP_ICON_HEIGHT + DESKTOP_ICON_PADDING;
			if(y + DESKTOP_ICON_HEIGHT > dims.height - DESKTOP_ICON_HEIGHT - DESKTOP_SANDBAR_HEIGHT) {
				y = DESKTOP_ICON_PADDING;
				x += DESKTOP_ICON_WIDTH + DESKTOP_ICON_PADDING;
			}
		}
	}
}

int DesktopWidget::icon_at(Gfx::Point pos) {
	for(int i = (int)m_icons.size() - 1; i >= 0; i--)
		if(pos.in(m_icons[i].rect)) return i;
	return -1;
}

void DesktopWidget::do_repaint(const DrawContext& ctx) {
	ctx.fill(ctx.rect(), {0, 0, 0, 0});
	layout_icons();

	for(auto& icon : m_icons) {
		if(icon.pressed)
			ctx.fill(icon.rect, {255, 255, 255, 60});
		else if(icon.hovered)
			ctx.fill(icon.rect, {255, 255, 255, 30});

		if(icon.rect.width == 0 || icon.rect.height == 0) continue;

		auto img = icon.app.icon();
		if(img) {
			int icon_x = icon.rect.x + (DESKTOP_ICON_WIDTH - DESKTOP_ICON_SIZE) / 2;
			int icon_y = icon.rect.y + 4;
			ctx.draw_image(img, Rect{icon_x, icon_y, DESKTOP_ICON_SIZE, DESKTOP_ICON_SIZE});
		}

		Rect label_rect = {
			icon.rect.x,
			icon.rect.y + DESKTOP_ICON_SIZE + 8,
			DESKTOP_ICON_WIDTH,
			DESKTOP_LABEL_HEIGHT
		};
		ctx.draw_text(icon.app.name().c_str(),
			Rect{label_rect.x + 1, label_rect.y + 1, label_rect.width, label_rect.height},
			CENTER, BEGINNING, UI::Theme::font(), {0, 0, 0, 180});
		ctx.draw_text(icon.app.name().c_str(),
			label_rect, CENTER, BEGINNING, UI::Theme::font(), {255, 255, 255, 255});
	}
}

bool DesktopWidget::on_mouse_move(Pond::MouseMoveEvent evt) {
	auto pos = evt.new_pos;

	if(m_pressed_icon >= 0) {
		int dx = abs(pos.x - m_press_pos.x);
		int dy = abs(pos.y - m_press_pos.y);

		if(!m_dragging && (dx > DESKTOP_DRAG_THRESHOLD || dy > DESKTOP_DRAG_THRESHOLD))
			m_dragging = true;

		if(m_dragging) {
			auto& rect   = m_icons[m_pressed_icon].rect;
			auto  screen = preferred_size();
			int new_x    = pos.x - m_drag_offset.x;
			int new_y    = pos.y - m_drag_offset.y;
			rect.x = std::max(0, std::min(new_x, screen.width  - DESKTOP_ICON_WIDTH));
			rect.y = std::max(0, std::min(new_y, screen.height - DESKTOP_ICON_HEIGHT - DESKTOP_SANDBAR_HEIGHT));
			repaint();
			return true;
		}
	}

	int hovered = icon_at(pos);
	if(hovered != m_last_hovered) {
		if(m_last_hovered >= 0 && m_last_hovered < (int)m_icons.size())
			m_icons[m_last_hovered].hovered = false;
		if(hovered >= 0 && hovered < (int)m_icons.size())
			m_icons[hovered].hovered = true;
		m_last_hovered = hovered;
		repaint();
	}
	return true;
}

bool DesktopWidget::on_mouse_button(Pond::MouseButtonEvent evt) {
	auto pos = mouse_position();

	if(!(evt.old_buttons & POND_MOUSE2) && (evt.new_buttons & POND_MOUSE2)) {
		int idx = icon_at(pos);
		if(idx < 0) return false;

		App::Info app_copy  = m_icons[idx].app;
		std::string app_name = app_copy.name();

		auto menu = UI::Menu::make();

		menu->add_item(UI::MenuItem::make("Open", [app_copy]() mutable {
			app_copy.run();
		}));

		menu->add_item(UI::MenuItem::Separator);

		// Use app name for removal (safer than index, which can change after reload)
		Duck::WeakPtr<DesktopWidget> self_weak = self<DesktopWidget>();
		menu->add_item(UI::MenuItem::make("Remove from Desktop", [self_weak, app_name]() {
			auto self = self_weak.lock();
			if(!self) return;
			// Find current index by name to avoid stale index
			for(int i = 0; i < (int)self->m_icons.size(); i++) {
				if(self->m_icons[i].app.name() == app_name) {
					self->remove_icon(i);
					return;
				}
			}
		}));

		open_menu(menu);
		return true;
	}

	if(!(evt.old_buttons & POND_MOUSE1) && (evt.new_buttons & POND_MOUSE1)) {
		int idx = icon_at(pos);

		if(m_pressed_icon >= 0 && m_pressed_icon < (int)m_icons.size())
			m_icons[m_pressed_icon].pressed = false;

		if(idx < 0) return false;

		m_pressed_icon = idx;
		m_press_pos    = pos;
		m_dragging     = false;
		m_icons[idx].pressed = true;
		m_drag_offset = {pos.x - m_icons[idx].rect.x, pos.y - m_icons[idx].rect.y};
		repaint();
	} else if((evt.old_buttons & POND_MOUSE1) && !(evt.new_buttons & POND_MOUSE1)) {
		if(m_pressed_icon >= 0 && m_pressed_icon < (int)m_icons.size()) {
			m_icons[m_pressed_icon].pressed = false;
			if(!m_dragging) {
				int idx = icon_at(pos);
				if(idx >= 0 && idx == m_pressed_icon)
					m_icons[idx].app.run();
			} else {
				// Persist the dragged position so it survives a reload
				auto& icon = m_icons[m_pressed_icon];
				m_icon_positions[icon.app.name()] = icon.rect.position();
			}
			repaint();
		}
		m_pressed_icon = -1;
		m_dragging     = false;
	}

	return true;
}

Gfx::Dimensions DesktopWidget::preferred_size() {
	auto dims = UI::pond_context->get_display_dimensions();
	return {dims.width, dims.height};
}