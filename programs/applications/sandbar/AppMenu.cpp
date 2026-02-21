/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2023 Byteduck */

#include "AppMenu.h"
#include <libui/widget/Image.h>
#include <libui/widget/Label.h>
#include <libui/widget/Button.h>
#include <libui/libui.h>
#include <libapp/App.h>
#include <sys/reboot.h>

using namespace UI;
using namespace Duck;

AppMenu::AppMenu():
	m_window(Window::make()),
	m_layout(BoxLayout::make(BoxLayout::VERTICAL))
{
	m_window->set_contents(m_layout);
	m_window->set_decorated(false);
	m_window->set_resizable(false);

	// === Daftar Aplikasi ===
	auto apps = App::get_all_apps();
	for(auto& app : apps) {
		if(app.hidden())
			continue;
		
		// Skip invalid apps
		if(app.name().empty())
			continue;
			
		auto btn_layout = UI::BoxLayout::make(UI::BoxLayout::HORIZONTAL, 4);
		
		// Safely get icon - gunakan std::move karena icon() non-const
		auto icon = std::move(app).icon();
		if(icon) {
			btn_layout->add_child(UI::Image::make(icon, UI::Image::FIT, Gfx::Dimensions {16, 16}));
		} else {
			// Gunakan missing icon placeholder
			btn_layout->add_child(UI::Image::make(LIBAPP_MISSING_ICON, UI::Image::FIT, Gfx::Dimensions {16, 16}));
		}
		
		auto btn_label = UI::Label::make(app.name());
		btn_label->set_alignment(UI::CENTER, UI::BEGINNING);
		btn_layout->add_child(btn_label);
		
		auto btn = UI::Button::make(btn_layout);
		btn->set_sizing_mode(UI::PREFERRED);
		btn->on_pressed = [app, this]{
			app.run();
			m_window->hide();
			m_shown = false;
		};
		m_layout->add_child(btn);
	}

	// === Tombol Reboot ===
	auto reboot_btn = UI::Button::make(UI::Label::make("Reboot"));
	reboot_btn->set_sizing_mode(UI::PREFERRED);
	reboot_btn->on_pressed = [this]{
		m_window->hide();
		m_shown = false;
		reboot(RB_AUTOBOOT);
	};
	m_layout->add_child(reboot_btn);

	// === Tombol Shutdown ===
	auto shutdown_btn = UI::Button::make(UI::Label::make("Shutdown"));
	shutdown_btn->set_sizing_mode(UI::PREFERRED);
	shutdown_btn->on_pressed = [this]{
		m_window->hide();
		m_shown = false;
		reboot(RB_POWER_OFF);
	};
	m_layout->add_child(shutdown_btn);

	m_window->resize(m_layout->preferred_size());
}

void AppMenu::show() {
	m_window->resize(m_layout->preferred_size());
	auto display_dims = UI::pond_context->get_display_dimensions();
	auto menu_height = m_window->dimensions().height;
	m_window->set_position({0, display_dims.height - menu_height - 36});
	m_window->show();
	m_window->bring_to_front();
	m_shown = true;
}

void AppMenu::hide() {
	m_window->hide();
	m_shown = false;
}

void AppMenu::toggle() {
	if(m_shown)
		hide();
	else
		show();
}

Duck::Ptr<UI::Window> AppMenu::window() {
	return m_window;
}