/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2025-2026 danko1122q */

#include "DesktopWidget.h"
#include <libui/libui.h>
#include <libui/DrawContext.h>
#include <libui/Theme.h>
#include <cstdlib>

using namespace UI;
using namespace Gfx;

DesktopWidget::DesktopWidget() {
	auto all_apps = App::get_all_apps();
	for(auto& app : all_apps) {
		if(app.hidden() || app.name().empty())
			continue;
		if(app.exec().empty() || !app.exists())
			continue;
		m_icons.push_back({app, {0, 0, 0, 0}});
	}
}

void DesktopWidget::layout_icons() {
	if(m_laid_out)
		return;
	m_laid_out = true;

	auto dims = preferred_size();
	int x = DESKTOP_ICON_PADDING;
	int y = DESKTOP_ICON_PADDING;

	for(auto& icon : m_icons) {
		icon.rect = {x, y, DESKTOP_ICON_WIDTH, DESKTOP_ICON_HEIGHT};
		y += DESKTOP_ICON_HEIGHT + DESKTOP_ICON_PADDING;
		if(y + DESKTOP_ICON_HEIGHT > dims.height - DESKTOP_ICON_HEIGHT - DESKTOP_SANDBAR_HEIGHT) {
			y = DESKTOP_ICON_PADDING;
			x += DESKTOP_ICON_WIDTH + DESKTOP_ICON_PADDING;
		}
	}
}

int DesktopWidget::icon_at(Gfx::Point pos) {
	for(int i = (int)m_icons.size() - 1; i >= 0; i--) {
		if(pos.in(m_icons[i].rect))
			return i;
	}
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

		if(icon.rect.width == 0 || icon.rect.height == 0)
			continue;

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
			auto& rect  = m_icons[m_pressed_icon].rect;
			auto  screen = preferred_size();

			int new_x = pos.x - m_drag_offset.x;
			int new_y = pos.y - m_drag_offset.y;

			int max_x = screen.width  - DESKTOP_ICON_WIDTH;
			int max_y = screen.height - DESKTOP_ICON_HEIGHT - DESKTOP_SANDBAR_HEIGHT;

			rect.x = std::max(0, std::min(new_x, max_x));
			rect.y = std::max(0, std::min(new_y, max_y));
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

	// ── Mouse DOWN ─────────────────────────────────────────────────────────
	if(!(evt.old_buttons & POND_MOUSE1) && (evt.new_buttons & POND_MOUSE1)) {
		int idx = icon_at(pos);

		if(m_pressed_icon >= 0 && m_pressed_icon < (int)m_icons.size())
			m_icons[m_pressed_icon].pressed = false;

		if(idx < 0)
			return false;

		m_pressed_icon = idx;
		m_press_pos    = pos;
		m_dragging     = false;

		m_icons[idx].pressed = true;
		m_drag_offset = {
			pos.x - m_icons[idx].rect.x,
			pos.y - m_icons[idx].rect.y
		};
		repaint();
	}
	// ── Mouse UP ────────────────────────────────────────────────────────────
	else if((evt.old_buttons & POND_MOUSE1) && !(evt.new_buttons & POND_MOUSE1)) {
		if(m_pressed_icon >= 0 && m_pressed_icon < (int)m_icons.size()) {
			m_icons[m_pressed_icon].pressed = false;

			// Kalau tidak drag dan masih di atas icon yang sama → langsung launch
			if(!m_dragging) {
				int idx = icon_at(pos);
				if(idx >= 0 && idx == m_pressed_icon)
					m_icons[idx].app.run();
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