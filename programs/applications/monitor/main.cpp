/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2025-2026 danko1122q */

#include <libui/libui.h>
#include <libui/widget/layout/BoxLayout.h>
#include <libui/widget/NamedCell.h>
#include <libui/widget/Cell.h>
#include "CpuGraphWidget.h"
#include "MemGraphWidget.h"

int main(int argc, char** argv, char** envp) {
    UI::init(argv, envp);

    auto cpu = CpuGraphWidget::make();
    auto mem = MemGraphWidget::make();

    // PREFERRED — grafik tidak stretch, selalu 200x50
    cpu->set_sizing_mode(UI::PREFERRED);
    mem->set_sizing_mode(UI::PREFERRED);

    auto cpu_cell = UI::NamedCell::make("CPU", cpu);
    auto mem_cell = UI::NamedCell::make("Memory", mem);
    // PREFERRED pada NamedCell juga agar tinggi tidak stretch
    cpu_cell->set_sizing_mode(UI::PREFERRED);
    mem_cell->set_sizing_mode(UI::PREFERRED);

    // Baris horizontal: CPU | Memory
    auto row = UI::BoxLayout::make(UI::BoxLayout::HORIZONTAL, 6);
    row->set_sizing_mode(UI::PREFERRED);
    row->add_child(cpu_cell);
    row->add_child(mem_cell);

    // Cell luar: memusatkan baris di tengah window
    // sizing FILL agar Cell mengisi window, tapi row di dalamnya tetap PREFERRED
    auto outer = UI::Cell::make(row, 6);
    outer->set_sizing_mode(UI::FILL);

    auto window = UI::Window::make();
    window->set_title("System Monitor");
    window->set_contents(outer);
    window->set_resizable(true);
    window->resize({450, 105});
    window->show();

    cpu->update();
    mem->update();

    auto timer = UI::set_interval([&cpu, &mem] {
        cpu->update();
        mem->update();
    }, 1000);

    UI::run();
    return 0;
}