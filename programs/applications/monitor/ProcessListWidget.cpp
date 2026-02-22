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

	Copyright (c) Byteduck 2016-2021. All rights reserved.
*/

#include "ProcessListWidget.h"
#include "ProcessManager.h"
#include "ProcessInspectorWidget.h"
#include <libui/widget/Image.h>
#include <libui/widget/Label.h>
#include <libui/Window.h>
#include <csignal>

void ProcessListWidget::update() {
	auto procs = ProcessManager::inst().processes();
	
	// Rebuild process list
	_processes.clear();
	for(auto& proc : procs) {
		_processes.push_back(proc.second);
	}
	
	// Update table view
	_table_view->update_data();
}

void ProcessListWidget::initialize() {
	set_sizing_mode(UI::FILL);
	_table_view->set_delegate(self());
	add_child(_table_view);
}

ProcessListWidget::ProcessListWidget() {
}

Duck::Ptr<UI::Widget> ProcessListWidget::tv_create_entry(int row, int col) {
	// FIX: Bounds check agar tidak crash kalau row out of range
	// (bisa terjadi kalau proses list berubah saat tabel sedang di-render)
	if(row < 0 || row >= (int)_processes.size())
		return UI::Label::make("");

	auto& proc = _processes[row];
	auto app_info = proc.app_info();
	switch(col) {
	case 0: // Icon
		if(app_info.has_value())
			return UI::Image::make(app_info.value().icon());
		else
			return UI::Label::make("");

	case 1: // PID
		return UI::Label::make(std::to_string(proc.pid()), UI::CENTER);

	case 2: // Name
		if(app_info.has_value())
			return UI::Label::make(app_info.value().name(), UI::BEGINNING);
		else
			return UI::Label::make(proc.name(), UI::BEGINNING);

	case 3: // Virtual
		return UI::Label::make(proc.virtual_mem().readable(), UI::BEGINNING);

	case 4: // Physical
		return UI::Label::make(proc.physical_mem().readable(), UI::BEGINNING);

	case 5: // Shared
		return UI::Label::make(proc.shared_mem().readable(), UI::BEGINNING);

	case 6: // State
		return UI::Label::make(proc.state_name(), UI::BEGINNING);
	}

	return nullptr;
}

std::string ProcessListWidget::tv_column_name(int col) {
	switch(col) {
		case 0:
			return "";
		case 1:
			return "PID";
		case 2:
			return "Name";
		case 3:
			return "Virtual";
		case 4:
			return "Physical";
		case 5:
			return "Shared";
		case 6:
			return "State";
	}
	return "";
}

int ProcessListWidget::tv_num_entries() {
	return _processes.size();
}

int ProcessListWidget::tv_row_height() {
	return 18;
}

int ProcessListWidget::tv_column_width(int col) {
	switch(col) {
		case 0:
			return 16;
		case 1:
			return 32;
		case 2:
			return -1;
		case 3:
			return 75;
		case 4:
			return 75;
		case 5:
			return 75;
		case 6:
			return 60;
	}
	return 0;
}

void ProcessListWidget::tv_selection_changed(const std::set<int>& selected_items) {
	// Intentionally empty — selection handled via context menu
}

Duck::Ptr<UI::Menu> ProcessListWidget::tv_entry_menu(int row) {
	// FIX: Bounds check — proses list bisa berubah saat menu dibuka
	if(row < 0 || row >= (int)_processes.size())
		return nullptr;

	auto process = _processes[row];
	return UI::Menu::make({
		UI::MenuItem::make("Kill", [process] {
			kill(process.pid(), SIGKILL);
		}),
		UI::MenuItem::make("Stop", [process] {
			kill(process.pid(), SIGSTOP);
		}),
		UI::MenuItem::make("Continue", [process] {
			kill(process.pid(), SIGCONT);
		}),
		UI::MenuItem::Separator,
		UI::MenuItem::make("Inspect", [process] {
			auto window = UI::Window::make();
			window->set_contents(ProcessInspectorWidget::make(process));
			window->set_title(process.name() + "(" + std::to_string(process.pid()) + ")");
			window->set_resizable(true);
			window->show();
		})
	});
}