/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2023 Byteduck */

#include "Sandbar.h"
#include <libui/libui.h>

Sandbar::Sandbar() {
	m_window = UI::Window::make();
	m_window->set_title("Sandbar");
	m_window->set_decorated(false);
	m_window->set_resizable(false);
	m_window->pond_window()->set_draggable(false);
	m_window->pond_window()->set_has_shadow(false);

	// Set tipe PANEL supaya sandbar selalu di atas window biasa
	// dan tidak bisa ditimpa saat window lain move_to_front
	m_window->pond_window()->set_type(Pond::PANEL);

	auto dims = UI::pond_context->get_display_dimensions();

	m_app_menu = AppMenu::make();
	m_widget = SandbarWidget::make(m_app_menu);
	m_window->set_contents(m_widget);

	m_window->set_position({0, dims.height - m_window->dimensions().height});
	m_window->resize({dims.width, m_window->dimensions().height});
	m_window->show();
}