/*
	This file is part of nusaOS.

	nusaOS is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	nusaOS is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with nusaOS.  If not, see <https://www.gnu.org/licenses/>.

	Copyright (c) nusaOS 2026. All rights reserved.
*/

#include "GameWidget.h"
#include <libui/libui.h>
#include <libui/widget/layout/BoxLayout.h>
#include <libui/widget/Button.h>
#include <libui/widget/Cell.h>

int main(int argc, char** argv, char** envp) {
	// Init LibUI
	UI::init(argv, envp);

	// Window — fixed size, matches original 540×400 + title bar
	auto window = UI::Window::make();
	window->set_title("floppybird");
	window->set_resizable(false);

	// Toolbar with a single "New Game" button (mirrors original start button)
	auto layout   = UI::BoxLayout::make(UI::BoxLayout::VERTICAL, 0);
	auto toolbar  = UI::BoxLayout::make(UI::BoxLayout::HORIZONTAL, 4);
	auto new_game = UI::Button::make("New Game");
	toolbar->add_child(new_game);

	auto game = GameWidget::make();

	new_game->on_pressed = [&] {
		game->reset();
	};

	layout->add_child(game);
	layout->add_child(toolbar);

	window->set_contents(UI::Cell::make(layout));
	window->show();

	// Run event loop
	UI::run();

	return 0;
}