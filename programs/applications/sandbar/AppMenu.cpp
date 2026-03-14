/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2023 Byteduck */

#include "AppMenu.h"
#include <libui/widget/Image.h>
#include <libui/widget/Label.h>
#include <libui/widget/Button.h>
#include <libui/widget/layout/BoxLayout.h>
#include <libui/widget/Widget.h>
#include <libui/widget/ListView.h>
#include <libui/Menu.h>
#include <libui/libui.h>
#include <libapp/App.h>
#include <sys/reboot.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>

using namespace UI;
using namespace Duck;

#define DESKTOP_PINS_FILE "/etc/nusa_desktop_pins"

static constexpr int MAX_VISIBLE_ITEMS = 10;
static constexpr int ITEM_HEIGHT       = 28;
static constexpr int ITEM_ICON_SIZE    = 16;
static constexpr int MENU_WIDTH        = 200;
static constexpr int HEADER_HEIGHT     = 28;
static constexpr int SEP_HEIGHT        = 1;
static constexpr int POWERBAR_HEIGHT   = 34;
static constexpr int SCROLLBAR_WIDTH   = 12;
static constexpr int ITEM_RIGHT_MARGIN = 2;

// ─────────────────────────────────────────────
//  AppMenuItemWidget
// ─────────────────────────────────────────────
class AppMenuItemWidget : public UI::Widget {
public:
	WIDGET_DEF(AppMenuItemWidget)

	std::function<void()> on_launch;
	App::Info app_info;

	bool on_mouse_button(Pond::MouseButtonEvent evt) override {
		if(!(evt.old_buttons & POND_MOUSE1) && (evt.new_buttons & POND_MOUSE1)) {
			m_pressed = true; repaint(); return true;
		}
		if((evt.old_buttons & POND_MOUSE1) && !(evt.new_buttons & POND_MOUSE1)) {
			bool was = m_pressed;
			m_pressed = false; repaint();
			if(was && m_hovered && on_launch) on_launch();
			return true;
		}
		if(!(evt.old_buttons & POND_MOUSE2) && (evt.new_buttons & POND_MOUSE2)) {
			show_context_menu(); return true;
		}
		return false;
	}

	bool on_mouse_move(Pond::MouseMoveEvent evt) override {
		// Kunci utama fix:
		// Jika ada mouse button yang ditekan (POND_MOUSE1) tapi
		// m_pressed = false, berarti klik itu BUKAN dari item ini
		// (melainkan dari scrollbar atau widget lain).
		// Dalam kondisi ini, jangan tampilkan hover sama sekali.
		if((mouse_buttons() & POND_MOUSE1) && !m_pressed) {
			if(m_hovered) { m_hovered = false; repaint(); }
			return false;
		}

		if(!m_hovered) { m_hovered = true; repaint(); }
		return false;
	}

	void on_mouse_leave(Pond::MouseLeaveEvent evt) override {
		m_hovered = false; m_pressed = false; repaint();
	}

	bool on_mouse_scroll(Pond::MouseScrollEvent evt) override { return false; }

	Gfx::Dimensions preferred_size() override {
		return {MENU_WIDTH - SCROLLBAR_WIDTH - ITEM_RIGHT_MARGIN, ITEM_HEIGHT};
	}

	void calculate_layout() override {
		if(!children.empty())
			children[0]->set_layout_bounds(
				Gfx::Rect{{4, 2}, current_size() - Gfx::Dimensions{8, 4}}
			);
	}

private:
	AppMenuItemWidget() = default;

	void do_repaint(const UI::DrawContext& ctx) override {
		if(m_pressed) {
			ctx.draw_inset_rect(ctx.rect(), UI::Theme::button());
		} else if(m_hovered) {
			ctx.fill(ctx.rect(), UI::Theme::button().lightened());
		} else {
			ctx.fill(ctx.rect(), UI::Theme::bg());
		}
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
		menu->add_item(UI::MenuItem::make("Open", [app_copy]() mutable { app_copy.run(); }));
		open_menu(menu);
	}

	bool m_hovered = false;
	bool m_pressed = false;
};

// ─────────────────────────────────────────────
//  AppMenuHeaderWidget
// ─────────────────────────────────────────────
class AppMenuHeaderWidget : public UI::Widget {
public:
	WIDGET_DEF(AppMenuHeaderWidget)
	Gfx::Dimensions preferred_size() override { return {MENU_WIDTH, HEADER_HEIGHT}; }
	void calculate_layout() override {
		if(!children.empty())
			children[0]->set_layout_bounds(Gfx::Rect{{6,4}, current_size() - Gfx::Dimensions{12,8}});
	}
private:
	AppMenuHeaderWidget() = default;
	void do_repaint(const UI::DrawContext& ctx) override {
		ctx.fill(ctx.rect(), UI::Theme::bg().darkened(0.85f));
		ctx.fill({0, ctx.height()-1, ctx.width(), 1}, UI::Theme::shadow_1());
	}
};

// ─────────────────────────────────────────────
//  AppMenuSeparatorWidget
// ─────────────────────────────────────────────
class AppMenuSeparatorWidget : public UI::Widget {
public:
	WIDGET_DEF(AppMenuSeparatorWidget)
	Gfx::Dimensions preferred_size() override { return {MENU_WIDTH, SEP_HEIGHT}; }
private:
	AppMenuSeparatorWidget() = default;
	void do_repaint(const UI::DrawContext& ctx) override {
		ctx.fill(ctx.rect(), UI::Theme::shadow_1());
	}
};

// ─────────────────────────────────────────────
//  AppMenuPowerBar
// ─────────────────────────────────────────────
class AppMenuPowerBar : public UI::Widget {
public:
	WIDGET_DEF(AppMenuPowerBar)
	Gfx::Dimensions preferred_size() override { return {MENU_WIDTH, POWERBAR_HEIGHT}; }
	void calculate_layout() override {
		if(!children.empty())
			children[0]->set_layout_bounds(Gfx::Rect{{4,3}, current_size() - Gfx::Dimensions{8,6}});
	}
private:
	AppMenuPowerBar() = default;
	void do_repaint(const UI::DrawContext& ctx) override {
		ctx.fill(ctx.rect(), UI::Theme::bg().darkened(0.9f));
	}
};

// ─────────────────────────────────────────────
//  AppMenuRoot — manual layout, no BoxLayout
// ─────────────────────────────────────────────
class AppMenuRoot : public UI::Widget {
public:
	WIDGET_DEF(AppMenuRoot)
	int list_height = MAX_VISIBLE_ITEMS * ITEM_HEIGHT;

	Gfx::Dimensions preferred_size() override {
		return {MENU_WIDTH, HEADER_HEIGHT + list_height + SEP_HEIGHT + POWERBAR_HEIGHT};
	}

	void calculate_layout() override {
		if(children.size() < 4) return;
		int y = 0;
		children[0]->set_layout_bounds({{0, y}, {MENU_WIDTH, HEADER_HEIGHT}});
		y += HEADER_HEIGHT;
		children[1]->set_layout_bounds({{0, y}, {MENU_WIDTH, list_height}});
		y += list_height;
		children[2]->set_layout_bounds({{0, y}, {MENU_WIDTH, SEP_HEIGHT}});
		y += SEP_HEIGHT;
		children[3]->set_layout_bounds({{0, y}, {MENU_WIDTH, POWERBAR_HEIGHT}});
	}

private:
	AppMenuRoot() = default;
	void do_repaint(const UI::DrawContext& ctx) override {
		ctx.fill(ctx.rect(), UI::Theme::bg());
	}
};

// ─────────────────────────────────────────────
//  AppMenuDelegate
// ─────────────────────────────────────────────
class AppMenuDelegate : public UI::ListViewDelegate {
public:
	std::vector<App::Info> apps;
	std::function<void(App::Info&)> on_launch;

	Duck::Ptr<UI::Widget> lv_create_entry(int index) override {
		auto& app = apps[index];
		auto row = UI::BoxLayout::make(UI::BoxLayout::HORIZONTAL, 6);

		auto icon = app.icon();
		if(icon)
			row->add_child(UI::Image::make(icon, UI::Image::FIT, Gfx::Dimensions{ITEM_ICON_SIZE, ITEM_ICON_SIZE}));
		else
			row->add_child(UI::Image::make(LIBAPP_MISSING_ICON, UI::Image::FIT, Gfx::Dimensions{ITEM_ICON_SIZE, ITEM_ICON_SIZE}));

		auto lbl = UI::Label::make(app.name());
		lbl->set_alignment(UI::CENTER, UI::BEGINNING);
		row->add_child(lbl);

		auto item = AppMenuItemWidget::make();
		item->app_info = app;
		item->set_sizing_mode(UI::PREFERRED);

		App::Info app_copy = app;
		auto cb = on_launch;
		item->on_launch = [app_copy, cb]() mutable { if(cb) cb(app_copy); };
		item->add_child(row);
		return item;
	}

	Gfx::Dimensions lv_preferred_item_dimensions() override {
		return {MENU_WIDTH - SCROLLBAR_WIDTH - ITEM_RIGHT_MARGIN, ITEM_HEIGHT};
	}

	int lv_num_items() override { return (int)apps.size(); }
};

// ─────────────────────────────────────────────
//  Helper: file pin
// ─────────────────────────────────────────────
static std::vector<std::string> read_pins_file() {
	std::vector<std::string> pins;
	FILE* f = fopen(DESKTOP_PINS_FILE, "r");
	if(!f) return pins;
	char buf[256];
	while(fgets(buf, sizeof(buf), f)) {
		size_t len = strlen(buf);
		while(len > 0 && (buf[len-1]=='\n' || buf[len-1]=='\r')) buf[--len]='\0';
		if(len > 0) pins.push_back(buf);
	}
	fclose(f);
	return pins;
}

static bool write_pins_file(const std::vector<std::string>& pins) {
	FILE* f = fopen(DESKTOP_PINS_FILE, "w");
	if(!f) return false;
	for(auto& p : pins) fprintf(f, "%s\n", p.c_str());
	fclose(f);
	return true;
}

void AppMenu::add_to_desktop(const std::string& app_name) {
	auto pins = read_pins_file();
	for(auto& p : pins) if(p == app_name) return;
	pins.push_back(app_name);
	write_pins_file(pins);
}

bool AppMenu::is_on_desktop(const std::string& app_name) {
	FILE* f = fopen(DESKTOP_PINS_FILE, "r");
	if(!f) return false;
	fclose(f);
	auto pins = read_pins_file();
	for(auto& p : pins) if(p == app_name) return true;
	return false;
}

// ─────────────────────────────────────────────
//  AppMenu constructor
// ─────────────────────────────────────────────
AppMenu::AppMenu():
	m_window(Window::make())
{
	m_window->set_decorated(false);
	m_window->set_resizable(false);

	auto delegate = std::make_shared<AppMenuDelegate>();
	auto all_apps = App::get_all_apps();
	for(auto& app : all_apps) {
		if(app.name().empty() || app.hidden()) continue;
		delegate->apps.push_back(app);
	}
	delegate->on_launch = [this](App::Info& app) {
		app.run();
		m_window->hide();
		m_shown = false;
	};
	m_delegate = delegate;

	int num_apps = (int)delegate->apps.size();
	int list_h   = std::min(num_apps, MAX_VISIBLE_ITEMS) * ITEM_HEIGHT;

	auto root = AppMenuRoot::make();
	root->list_height = list_h;

	// child[0] — Header
	auto hdr = AppMenuHeaderWidget::make();
	auto hdr_inner = UI::BoxLayout::make(UI::BoxLayout::HORIZONTAL, 6);
	auto hdr_lbl = UI::Label::make("Applications");
	hdr_lbl->set_alignment(UI::CENTER, UI::CENTER);
	hdr_inner->add_child(hdr_lbl);
	hdr->add_child(hdr_inner);
	root->add_child(hdr);

	// child[1] — ListView
	auto list_view = UI::ListView::make();
	list_view->delegate = delegate;
	list_view->set_sizing_mode(UI::FILL);
	root->add_child(list_view);

	// child[2] — Separator
	root->add_child(AppMenuSeparatorWidget::make());

	// child[3] — Power bar
	auto power_row = UI::BoxLayout::make(UI::BoxLayout::HORIZONTAL, 4);
	power_row->set_sizing_mode(UI::FILL);

	auto reboot_btn = UI::Button::make(UI::Label::make("Reboot"));
	reboot_btn->set_sizing_mode(UI::FILL);
	reboot_btn->on_pressed = [this] { m_window->hide(); m_shown=false; reboot(RB_AUTOBOOT); };
	power_row->add_child(reboot_btn);

	auto shutdown_btn = UI::Button::make(UI::Label::make("Shut Down"));
	shutdown_btn->set_sizing_mode(UI::FILL);
	shutdown_btn->on_pressed = [this] { m_window->hide(); m_shown=false; reboot(RB_POWER_OFF); };
	power_row->add_child(shutdown_btn);

	auto power_bar = AppMenuPowerBar::make();
	power_bar->add_child(power_row);
	root->add_child(power_bar);

	m_window->set_contents(root);
	int total_h = HEADER_HEIGHT + list_h + SEP_HEIGHT + POWERBAR_HEIGHT;
	m_window->resize({MENU_WIDTH, total_h});
}

void AppMenu::show() {
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