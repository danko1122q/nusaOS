/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2025-2026 danko1122q */

#include "ViewerWidget.h"
#include <algorithm>

ViewerWidget::ViewerWidget(const Duck::Ptr<Gfx::Image>& image):
	m_image(image),
	m_image_rect({0, 0, image->size()})
	{}

void ViewerWidget::do_repaint(const UI::DrawContext& ctx) {
	ctx.fill(ctx.rect(), UI::Theme::bg());

	int sw = std::max(1, (int)(m_image_rect.width  * m_scale_factor));
	int sh = std::max(1, (int)(m_image_rect.height * m_scale_factor));

	// Guard: jangan draw jika posisi benar-benar di luar layar
	// (bisa terjadi saat on_layout_change dipanggil sebelum ukuran valid)
	Gfx::Rect scaled_rect = {m_image_rect.x, m_image_rect.y, sw, sh};
	ctx.draw_image(m_image, scaled_rect);
}

void ViewerWidget::on_layout_change(const Gfx::Rect& old_rect) {
	auto sz = current_size();
	// Guard: jangan center jika belum punya ukuran valid
	if (sz.width <= 0 || sz.height <= 0)
		return;

	int sw = std::max(1, (int)(m_image_rect.width  * m_scale_factor));
	int sh = std::max(1, (int)(m_image_rect.height * m_scale_factor));
	Gfx::Rect scaled_rect = {m_image_rect.x, m_image_rect.y, sw, sh};
	auto centered = scaled_rect.centered_on(Gfx::Rect {0, 0, sz}.center());
	m_image_rect.set_position(centered.position());
}

Gfx::Dimensions ViewerWidget::preferred_size() {
	return {
		std::max(1, (int)(m_image_rect.width  * m_scale_factor)),
		std::max(1, (int)(m_image_rect.height * m_scale_factor))
	};
}

bool ViewerWidget::on_mouse_button(Pond::MouseButtonEvent evt) {
	m_mouse_buttons = evt.new_buttons;
	return true;
}

bool ViewerWidget::on_mouse_scroll(Pond::MouseScrollEvent evt) {
	double old_scale = m_scale_factor;
	m_scale_factor -= evt.scroll * m_scale_factor * 0.1;
	m_scale_factor  = std::clamp(m_scale_factor, 0.05, 100.0);

	double ratio = m_scale_factor / old_scale;
	m_image_rect.x = (int)(m_mouse_pos.x - (m_mouse_pos.x - m_image_rect.x) * ratio);
	m_image_rect.y = (int)(m_mouse_pos.y - (m_mouse_pos.y - m_image_rect.y) * ratio);

	repaint();
	return true;
}

bool ViewerWidget::on_mouse_move(Pond::MouseMoveEvent evt) {
	m_mouse_pos = evt.new_pos;

	if (m_mouse_buttons & POND_MOUSE1) {
		m_image_rect.x += evt.delta.x;
		m_image_rect.y += evt.delta.y;
		repaint();
	}
	return true;
}