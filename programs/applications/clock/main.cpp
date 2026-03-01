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

	Copyright (c) danko1122q 2026. All rights reserved.
*/

#include "ClockWidget.h"
#include <libui/libui.h>
#include <libui/widget/Cell.h>

int main(int argc, char** argv, char** envp) {
	UI::init(argv, envp);

	auto window = UI::Window::make();
	window->set_title("Time & Date");
	window->set_resizable(false);

	auto clock = ClockWidget::make();
	window->set_contents(UI::Cell::make(clock, 0));
	window->show();

	UI::run();
	return 0;
}