/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2023 Byteduck */

#include "SandbarWidget.h"
#include "Sandbar.h"
#include "modules/TimeModule.h"
#include "modules/CPUModule.h"
#include "modules/MemoryModule.h"
#include <libui/widget/Cell.h>
#include <libui/widget/Stack.h>
#include <libui/widget/Image.h>
#include <libui/libui.h>

using namespace UI;
using namespace Duck;

SandbarWidget::SandbarWidget(Duck::Ptr<AppMenu> app_menu):
	m_layout(FlexLayout::make(FlexLayout::HORIZONTAL)),
	m_app_menu(app_menu)
{
	add_child(Cell::make(m_layout));

	// Icon bebek untuk tombol Apps - gunakan /nusa seperti aslinya
	// Jika /nusa tidak ada, fallback ke missing icon
	Duck::Ptr<UI::Image> nusa_icon;
	auto test_img = UI::Image::make("/nusa");
	if(test_img) {
		nusa_icon = test_img;
	} else {
		nusa_icon = UI::Image::make(LIBAPP_MISSING_ICON);
	}
	
	m_nusa_button = UI::Button::make(UI::Stack::make(UI::Stack::HORIZONTAL, 4, nusa_icon, UI::Label::make("Apps ")));
	m_nusa_button->set_sizing_mode(UI::PREFERRED);
	m_nusa_button->set_style(ButtonStyle::RAISED);
	m_nusa_button->on_pressed = [&] {
		m_app_menu->toggle();
	};

	m_layout->add_child(m_nusa_button);
	m_layout->add_child(Cell::make());

	auto add_module = [&](Duck::Ptr<Module> module) {
		m_layout->add_child(module);
		m_modules.push_back(module);
	};

	add_module(MemoryModule::make());
	add_module(CPUModule::make());
	add_module(TimeModule::make());

	m_module_timer = UI::set_interval([&]() {
		for(auto& module : m_modules)
			module->update();
	}, 1000);
}

void SandbarWidget::do_repaint(const DrawContext& ctx) {
	ctx.fill(ctx.rect(), UI::Theme::bg());
}

Gfx::Dimensions SandbarWidget::preferred_size() {
	return {100, Sandbar::HEIGHT};
}