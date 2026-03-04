/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2023 Byteduck */

#include "AppMenu.h"
#include <libui/widget/Image.h>
#include <libui/widget/Label.h>
#include <libui/widget/Button.h>
#include <libui/widget/layout/BoxLayout.h>
#include <libui/widget/Widget.h>
#include <libui/Menu.h>
#include <libui/libui.h>
#include <libapp/App.h>
#include <sys/reboot.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

using namespace UI;
using namespace Duck;

#define DESKTOP_PINS_FILE "/etc/nusa_desktop_pins"

class AppMenuItemWidget : public UI::Widget {
public:
	WIDGET_DEF(AppMenuItemWidget)

	std::function<void()> on_launch;
	App::Info app_info;

	bool on_mouse_button(Pond::MouseButtonEvent evt) override {
		if(!(evt.old_buttons & POND_MOUSE1) && (evt.new_buttons & POND_MOUSE1)) {
			m_pressed = true;
			repaint();
			return true;
		}
		if((evt.old_buttons & POND_MOUSE1) && !(evt.new_buttons & POND_MOUSE1)) {
			m_pressed = false;
			repaint();
			if(on_launch) on_launch();
			return true;
		}
		if(!(evt.old_buttons & POND_MOUSE2) && (evt.new_buttons & POND_MOUSE2)) {
			show_context_menu();
			return true;
		}
		return false;
	}

	bool on_mouse_move(Pond::MouseMoveEvent evt) override {
		if(!m_hovered) { m_hovered = true; repaint(); }
		return false;
	}

	void on_mouse_leave(Pond::MouseLeaveEvent evt) override {
		m_hovered = false;
		m_pressed = false;
		repaint();
	}

	Gfx::Dimensions preferred_size() override {
		if(!children.empty())
			return children[0]->preferred_size() + Gfx::Dimensions{0, 4};
		return {120, 24};
	}

	void calculate_layout() override {
		if(!children.empty())
			children[0]->set_layout_bounds(
				Gfx::Rect{{0, 2}, current_size() - Gfx::Dimensions{0, 4}}
			);
	}

private:
	AppMenuItemWidget() = default;

	void do_repaint(const UI::DrawContext& ctx) override {
		if(m_pressed)
			ctx.draw_inset_rect(ctx.rect(), UI::Theme::button());
		else if(m_hovered)
			ctx.draw_outset_rect(ctx.rect(), UI::Theme::button().lightened());
		else
			ctx.fill(ctx.rect(), UI::Theme::bg());
	}

	void show_context_menu() {
		auto menu = UI::Menu::make();

		if(!AppMenu::is_on_desktop(app_info.name())) {
			std::string name = app_info.name();
			menu->add_item(UI::MenuItem::make("Pin to Desktop", [name]() {
				AppMenu::add_to_desktop(name);
			}));
		} else {
			menu->add_item(UI::MenuItem::make("Already on Desktop", UI::MenuItem::Action{}));
		}

		menu->add_item(UI::MenuItem::Separator);

		App::Info app_copy = app_info;
		menu->add_item(UI::MenuItem::make("Open", [app_copy]() mutable {
			app_copy.run();
		}));

		open_menu(menu);
	}

	bool m_hovered = false;
	bool m_pressed = false;
};

static std::vector<std::string> read_pins_file() {
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

static bool write_pins_file(const std::vector<std::string>& pins) {
	FILE* f = fopen(DESKTOP_PINS_FILE, "w");
	if(!f) return false;
	for(auto& p : pins)
		fprintf(f, "%s\n", p.c_str());
	fclose(f);
	return true;
}

void AppMenu::add_to_desktop(const std::string& app_name) {
	auto pins = read_pins_file();
	for(auto& p : pins)
		if(p == app_name) return;
	pins.push_back(app_name);
	write_pins_file(pins);
}

bool AppMenu::is_on_desktop(const std::string& app_name) {
	FILE* f = fopen(DESKTOP_PINS_FILE, "r");
	if(!f) return false;  // File doesn't exist = nothing is pinned
	fclose(f);
	auto pins = read_pins_file();
	for(auto& p : pins)
		if(p == app_name) return true;
	return false;
}

AppMenu::AppMenu():
	m_window(Window::make()),
	m_layout(BoxLayout::make(BoxLayout::VERTICAL))
{
	m_window->set_contents(m_layout);
	m_window->set_decorated(false);
	m_window->set_resizable(false);

	auto apps = App::get_all_apps();
	for(auto& app : apps) {
		if(app.name().empty() || app.hidden())
			continue;

		auto row = UI::BoxLayout::make(UI::BoxLayout::HORIZONTAL, 4);

		auto icon = app.icon();
		if(icon)
			row->add_child(UI::Image::make(icon, UI::Image::FIT, Gfx::Dimensions{16, 16}));
		else
			row->add_child(UI::Image::make(LIBAPP_MISSING_ICON, UI::Image::FIT, Gfx::Dimensions{16, 16}));

		auto lbl = UI::Label::make(app.name());
		lbl->set_alignment(UI::CENTER, UI::BEGINNING);
		row->add_child(lbl);

		auto item = AppMenuItemWidget::make();
		item->app_info = app;
		item->set_sizing_mode(UI::PREFERRED);

		App::Info app_copy = app;
		item->on_launch = [app_copy, this]() mutable {
			app_copy.run();
			m_window->hide();
			m_shown = false;
		};

		item->add_child(row);
		m_layout->add_child(item);
	}

	m_layout->add_child(UI::BoxLayout::make(UI::BoxLayout::HORIZONTAL));

	auto reboot_btn = UI::Button::make(UI::Label::make("Reboot"));
	reboot_btn->set_sizing_mode(UI::PREFERRED);
	reboot_btn->on_pressed = [this] {
		m_window->hide();
		m_shown = false;
		reboot(RB_AUTOBOOT);
	};
	m_layout->add_child(reboot_btn);

	auto shutdown_btn = UI::Button::make(UI::Label::make("Shut Down"));
	shutdown_btn->set_sizing_mode(UI::PREFERRED);
	shutdown_btn->on_pressed = [this] {
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
	m_window->set_position({0, display_dims.height - m_window->dimensions().height - 36});
	m_window->show();
	m_window->bring_to_front();
	m_shown = true;
}

void AppMenu::hide() {
	m_window->hide();
	m_shown = false;
}

void AppMenu::toggle() {
	if(m_shown) hide();
	else show();
}

Duck::Ptr<UI::Window> AppMenu::window() {
	return m_window;
}