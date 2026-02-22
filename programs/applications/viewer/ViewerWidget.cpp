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

	// FIX Bug 1: Hitung scaled rect yang benar dari posisi + dimensi * scale_factor.
	// Jangan pakai Rect::scaled() karena itu melakukan geometric inset dari pusat,
	// bukan scaling dimensi gambar.
	Gfx::Rect scaled_rect = {
		m_image_rect.x,
		m_image_rect.y,
		(int)(m_image_rect.width * m_scale_factor),
		(int)(m_image_rect.height * m_scale_factor)
	};

	// Pastikan rect valid sebelum menggambar (hindari akses memori out-of-bounds)
	if(scaled_rect.width > 0 && scaled_rect.height > 0)
		ctx.draw_image(m_image, scaled_rect);
}

void ViewerWidget::on_layout_change(const Gfx::Rect& old_rect) {
	// FIX Bug 2: Gunakan dimensi yang sudah di-scale untuk penghitungan centering,
	// agar posisi gambar akurat saat scale_factor berubah.
	Gfx::Rect scaled_rect = {
		m_image_rect.x,
		m_image_rect.y,
		(int)(m_image_rect.width * m_scale_factor),
		(int)(m_image_rect.height * m_scale_factor)
	};
	auto centered_rect = scaled_rect.centered_on(Gfx::Rect {0, 0, current_size()}.center());
	m_image_rect.set_position(centered_rect.position());
}

Gfx::Dimensions ViewerWidget::preferred_size() {
	// FIX Bug 3: Cast ke int secara eksplisit setelah perkalian double
	// agar tidak terjadi truncation tak sengaja (misal 0.5 → 0).
	// Juga pastikan minimal 1x1 agar window tidak collapse.
	int w = std::max(1, (int)(m_image_rect.width * m_scale_factor));
	int h = std::max(1, (int)(m_image_rect.height * m_scale_factor));
	return {w, h};
}

bool ViewerWidget::on_mouse_scroll(Pond::MouseScrollEvent evt) {
	m_scale_factor -= evt.scroll * m_scale_factor * 0.1;
	m_scale_factor = std::clamp(m_scale_factor, 0.01, 100.0);
	// Trigger layout ulang agar centering dihitung ulang sesuai scale baru
	repaint();
	return true;
}

bool ViewerWidget::on_mouse_move(Pond::MouseMoveEvent evt) {
	return true;
}