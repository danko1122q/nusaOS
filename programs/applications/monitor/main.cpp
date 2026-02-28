/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2026 nusaOS */

#include <libui/libui.h>
#include <libui/widget/layout/BoxLayout.h>
#include <libui/widget/Cell.h>
#include <libui/widget/NamedCell.h>
#include "CpuGraphWidget.h"
#include "MemGraphWidget.h"
#include "ProcessListWidget.h"
#include "ProcessManager.h"

int main(int argc, char** argv, char** envp) {
	UI::init(argv, envp);

	// --- Grafik CPU dan RAM ---
	auto cpu_graph = CpuGraphWidget::make();
	auto mem_graph = MemGraphWidget::make();

	// Grafik dibuat FILL horizontal supaya memenuhi lebar window saat di-resize
	cpu_graph->set_sizing_mode(UI::FILL);
	mem_graph->set_sizing_mode(UI::FILL);

	auto graphs_row = UI::BoxLayout::make(UI::BoxLayout::HORIZONTAL, 4);
	graphs_row->add_child(UI::NamedCell::make("CPU", cpu_graph));
	graphs_row->add_child(UI::NamedCell::make("Memory", mem_graph));

	// Row grafik: preferred height (tidak ikut stretch ke bawah)
	auto graphs_cell = UI::Cell::make(graphs_row);
	graphs_cell->set_sizing_mode(UI::PREFERRED);

	// --- Process list (fill sisa ruang vertikal) ---
	auto proc_list = ProcessListWidget::make();
	auto proc_cell = UI::NamedCell::make("Processes", proc_list);
	proc_cell->set_sizing_mode(UI::FILL);

	// --- Layout utama ---
	auto layout = UI::BoxLayout::make(UI::BoxLayout::VERTICAL, 4);
	layout->add_child(graphs_cell);
	layout->add_child(proc_cell);

	auto window = UI::Window::make();
	window->set_title("System Monitor");
	window->set_contents(layout);
	window->set_resizable(true);
	window->resize({620, 440});
	window->show();

	// Satu timer, semua update di satu tempat
	auto timer = UI::set_interval([&] {
		cpu_graph->update();
		mem_graph->update();
		ProcessManager::inst().update();
		proc_list->update();
	}, 1000);

	UI::run();
	return 0;
}