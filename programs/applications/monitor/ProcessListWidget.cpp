/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2026 nusaOS */

#include "ProcessListWidget.h"
#include "ProcessManager.h"
#include "ProcessInspectorWidget.h"
#include <libui/widget/Label.h>
#include <libui/Window.h>
#include <csignal>

ProcessListWidget::ProcessListWidget() {}

void ProcessListWidget::initialize() {
	set_sizing_mode(UI::FILL);
	_table_view->set_delegate(self());
	add_child(_table_view);
}

void ProcessListWidget::update() {
	_processes.clear();
	for (auto& kv : ProcessManager::inst().processes())
		_processes.push_back(kv.second);
	_table_view->update_data();
}

Duck::Ptr<UI::Widget> ProcessListWidget::tv_create_entry(int row, int col) {
	if (row < 0 || row >= (int)_processes.size())
		return UI::Label::make("");

	auto& proc = _processes[row];

	switch (col) {
	case 0: // PID
		return UI::Label::make(std::to_string(proc.pid()), UI::CENTER);
	case 1: { // Name
		auto info = proc.app_info();
		std::string name = info.has_value() ? info.value().name() : proc.name();
		return UI::Label::make(name, UI::BEGINNING);
	}
	case 2: // Virtual
		return UI::Label::make(proc.virtual_mem().readable(), UI::BEGINNING);
	case 3: // Physical
		return UI::Label::make(proc.physical_mem().readable(), UI::BEGINNING);
	case 4: // Shared
		return UI::Label::make(proc.shared_mem().readable(), UI::BEGINNING);
	case 5: // State
		return UI::Label::make(proc.state_name(), UI::CENTER);
	case 6: // dummy — header masih 7 kolom di .h lama
		return UI::Label::make("");
	}
	return UI::Label::make("");
}

std::string ProcessListWidget::tv_column_name(int col) {
	switch (col) {
	case 0: return "PID";
	case 1: return "Name";
	case 2: return "Virtual";
	case 3: return "Physical";
	case 4: return "Shared";
	case 5: return "State";
	}
	return "";
}

int ProcessListWidget::tv_num_entries()  { return _processes.size(); }
int ProcessListWidget::tv_row_height()   { return 18; }

int ProcessListWidget::tv_column_width(int col) {
	switch (col) {
	case 0: return 36;   // PID
	case 1: return -1;   // Name — fill sisa
	case 2: return 68;   // Virtual
	case 3: return 68;   // Physical
	case 4: return 60;   // Shared
	case 5: return 60;   // State
	case 6: return 0;
	}
	return 0;
}

void ProcessListWidget::tv_selection_changed(const std::set<int>&) {}

Duck::Ptr<UI::Menu> ProcessListWidget::tv_entry_menu(int row) {
	if (row < 0 || row >= (int)_processes.size())
		return nullptr;

	auto process = _processes[row];
	return UI::Menu::make({
		UI::MenuItem::make("Kill", [process] { kill(process.pid(), SIGKILL); }),
		UI::MenuItem::make("Stop", [process] { kill(process.pid(), SIGSTOP); }),
		UI::MenuItem::make("Continue", [process] { kill(process.pid(), SIGCONT); }),
		UI::MenuItem::Separator,
		UI::MenuItem::make("Inspect", [process] {
			auto w = UI::Window::make();
			w->set_contents(ProcessInspectorWidget::make(process));
			w->set_title(process.name() + " (" + std::to_string(process.pid()) + ")");
			w->set_resizable(true);
			w->show();
		})
	});
}