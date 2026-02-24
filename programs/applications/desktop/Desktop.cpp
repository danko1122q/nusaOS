/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2025-2026 danko1122q */

#include "Desktop.h"
#include <libui/libui.h>

Desktop::Desktop() {
	m_window = UI::Window::make();
	m_window->set_title("Desktop");
	m_window->set_decorated(false);
	m_window->set_resizable(false);
	m_window->pond_window()->set_draggable(false);
	m_window->pond_window()->set_has_shadow(false);

	// Set tipe DESKTOP supaya pond tahu window ini harus selalu di layer bawah
	m_window->pond_window()->set_type(Pond::DESKTOP);

	auto dims = UI::pond_context->get_display_dimensions();
	m_widget = DesktopWidget::make();
	m_window->set_contents(m_widget);
	// Aktifkan alpha blending supaya background transparent widget menembus ke wallpaper
	m_window->pond_window()->set_uses_alpha(true);
	m_window->resize({dims.width, dims.height});
	m_window->set_position({0, 0});
	m_window->show();
}