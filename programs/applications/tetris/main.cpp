/*
	This file is part of nusaOS.
	SPDX-License-Identifier: GPL-3.0-or-later
	Copyright (c) danko1122q 2026. All rights reserved.
*/

#include <libui/libui.h>
#include <libui/Window.h>
#include <libui/widget/Cell.h>
#include "TetrisWidget.h"

int main(int argc, char** argv, char** envp) {
	UI::init(argv, envp);

	auto window = UI::Window::make();
	window->set_title("Tetris");
	window->set_resizable(false);

	auto widget = TetrisWidget::make();
	// Cell::make(child, padding=0, background, style)
	auto cell = UI::Cell::make(widget, 0);

	window->set_contents(cell);
	window->resize_to_contents();
	window->show();

	UI::run();
	return 0;
}