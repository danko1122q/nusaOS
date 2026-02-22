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

#include <libui/libui.h>
#include <libui/widget/ProgressBar.h>
#include <libui/widget/Label.h>
#include <libui/widget/layout/BoxLayout.h>
#include <sys/time.h>
#include <libsys/Memory.h>
#include <libsys/CPU.h>
#include "ProcessListWidget.h"
#include "MemoryUsageWidget.h"
#include "ProcessManager.h"
#include <libui/widget/Cell.h>
#include <libnusa/FileStream.h>

#define UPDATE_FREQ 1000

using namespace Sys;

Duck::Ptr<UI::ProgressBar> cpu_bar;
Duck::Ptr<MemoryUsageWidget> mem_widget;
Duck::Ptr<UI::Label> mem_label;
Duck::Ptr<ProcessListWidget> proc_list;

Duck::FileInputStream cpu_stream;
Duck::FileInputStream mem_stream;

CPU::Info cpu_info;
Mem::Info mem_info;

Duck::Result update() {
	static timeval last_update = {0, 0};

	timeval tv = {0, 0};
	gettimeofday(&tv, nullptr);
	int diff = (int) (((tv.tv_sec - last_update.tv_sec) * 1000000) + (tv.tv_usec - last_update.tv_usec))/1000;
	if(diff < UPDATE_FREQ && last_update.tv_sec != 0)
		return Duck::Result::SUCCESS;
	last_update = tv;

	if(!mem_widget || !mem_label || !cpu_bar || !proc_list)
		return Duck::Result::SUCCESS;

	auto cpu_res = CPU::get_info(cpu_stream);
	if(cpu_res.is_error())
		return Duck::Result::SUCCESS;
	cpu_info = cpu_res.value();

	auto mem_res = Mem::get_info(mem_stream);
	if(mem_res.is_error())
		return Duck::Result::SUCCESS;
	mem_info = mem_res.value();

	mem_widget->update(mem_info);

	std::string mem_text = "Kernel: " + Mem::Amount {mem_info.kernel_phys - mem_info.kernel_disk_cache}.readable();
	mem_text += " / Disk Cache: " + mem_info.kernel_disk_cache.readable();
	mem_text += " / User: " + Mem::Amount {mem_info.used - mem_info.kernel_virt}.readable();
	mem_label->set_label(mem_text);

	cpu_bar->set_progress(cpu_info.utilization / 100.0);
	cpu_bar->set_label("CPU: " + std::to_string(cpu_info.utilization) + "%");

	// FIX: Update ProcessManager DULU sebelum proc_list->update()
	// agar snapshot map di ProcessInspectorWidget::update() konsisten
	// dengan data yang baru di-fetch. Urutan sebelumnya sama, tapi
	// sekarang eksplisit untuk kejelasan.
	ProcessManager::inst().update();
	proc_list->update();

	return Duck::Result::SUCCESS;
}

int main(int argc, char** argv, char** envp) {
	auto res = cpu_stream.open("/proc/cpuinfo");
	if(res.is_error()) {
		perror("Failed to open cpuinfo");
		return res.code();
	}

	res = mem_stream.open("/proc/meminfo");
	if(res.is_error()) {
		Duck::Log::err("Failed to open meminfo");
		exit(res.code());
	}

	UI::init(argv, envp);

	auto window = UI::Window::make();
	window->set_title("System Monitor");

	mem_widget = MemoryUsageWidget::make();
	mem_label = UI::Label::make("");
	cpu_bar = UI::ProgressBar::make();

	auto layout = UI::BoxLayout::make(UI::BoxLayout::VERTICAL, 0);
	layout->add_child(UI::Cell::make(cpu_bar));
	layout->add_child(UI::Cell::make(mem_widget));
	layout->add_child(UI::Cell::make(mem_label));

	proc_list = ProcessListWidget::make();
	layout->add_child(UI::Cell::make(proc_list));

	window->set_contents(layout);
	window->set_resizable(true);
	
	// Set a reasonable default window size
	window->resize({600, 400});
	
	window->show();

	// Load data AFTER showing window to ensure widgets are fully initialized
	// Add small delay to ensure window system is ready
	UI::set_timeout([&]{
		update();
		// Force layout update after data loads
		window->resize(window->dimensions());
	}, 100);

	auto timer = UI::set_interval(update, UPDATE_FREQ);
	UI::run();
}