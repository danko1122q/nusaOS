/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2023 Byteduck */

#pragma once

#include <libui/Window.h>
#include <libui/widget/layout/BoxLayout.h>
#include <string>

class AppMenu: public Duck::Object {
public:
	NUSA_OBJECT_DEF(AppMenu);

	void show();
	void hide();
	void toggle();

	[[nodiscard]] Duck::Ptr<UI::Window> window();

	static void add_to_desktop(const std::string& app_name);
	static bool is_on_desktop(const std::string& app_name);

private:
	AppMenu();

	bool m_shown = false;
	Duck::Ptr<UI::Window> m_window;
	Duck::Ptr<UI::BoxLayout> m_layout;
};