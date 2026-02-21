/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2025-2026 danko1122q */

#include <libui/libui.h>
#include <libui/widget/layout/FlexLayout.h>
#include <libui/widget/Label.h>
#include <libui/widget/Cell.h>
#include <libsys/Memory.h>
#include <libnusa/StringStream.h>
#include <sys/utsname.h>
#include <cstdio>

using namespace Sys;

static std::string get_uptime_str() {
    FILE* f = fopen("/proc/uptime", "r");
    if (!f) return "N/A";
    long seconds = -1;
    fscanf(f, "%ld", &seconds);
    fclose(f);
    if (seconds < 0) return "N/A";
    long days    = seconds / 86400;
    long hours   = (seconds % 86400) / 3600;
    long minutes = (seconds % 3600) / 60;
    long secs    = seconds % 60;
    char buf[64];
    if (days > 0)
        snprintf(buf, sizeof(buf), "%ldd %02ld:%02ld:%02ld", days, hours, minutes, secs);
    else
        snprintf(buf, sizeof(buf), "%02ld:%02ld:%02ld", hours, minutes, secs);
    return std::string(buf);
}

int main(int argc, char** argv, char** envp) {
    UI::init(argv, envp);

    utsname uname_buf;
    if (uname(&uname_buf))
        exit(-1);

    // Ambil info memory
    std::string mem_str = "N/A";
    auto mem_res = Mem::get_info();
    if (!mem_res.is_error()) {
        auto& mem = mem_res.value();
        Duck::StringOutputStream ss;
        ss << mem.used.readable() << " / " << mem.usable.readable();
        mem_str = ss.string();
    }

    // Ambil uptime
    std::string uptime_str = get_uptime_str();

    auto window = UI::Window::make();
    window->set_title("About " + std::string(uname_buf.sysname));

    auto layout = UI::FlexLayout::make(UI::FlexLayout::VERTICAL);
    layout->set_spacing(6);

    // Nama OS — judul besar
    auto title = UI::Label::make(uname_buf.sysname);
    title->set_font(UI::pond_context->get_font("gohu-14"));
    title->set_sizing_mode(UI::PREFERRED);
    title->set_color(RGB(0, 0, 0));
    layout->add_child(title);

    // Garis pemisah
    auto sep = UI::Label::make("--------------------");
    sep->set_color(RGB(150, 150, 150));
    sep->set_sizing_mode(UI::PREFERRED);
    layout->add_child(sep);

    // Helper lambda untuk buat baris info
    auto make_row = [&](const std::string& label, const std::string& value) {
        Duck::StringOutputStream ss;
        ss << label << value;
        auto lbl = UI::Label::make(ss.string());
        lbl->set_color(RGB(50, 50, 50));
        lbl->set_sizing_mode(UI::PREFERRED);
        layout->add_child(lbl);
    };

    make_row("Version:  ", uname_buf.release);
    make_row("Arch:     ", uname_buf.machine);
    make_row("Uptime:   ", uptime_str);
    make_row("Memory:   ", mem_str);

    // Garis pemisah bawah
    auto sep2 = UI::Label::make("--------------------");
    sep2->set_color(RGB(150, 150, 150));
    sep2->set_sizing_mode(UI::PREFERRED);
    layout->add_child(sep2);

    // Copyright
    auto copy = UI::Label::make("© 2025-2026 danko1122q");
    copy->set_color(RGB(120, 120, 120));
    copy->set_sizing_mode(UI::PREFERRED);
    layout->add_child(copy);

    window->set_contents(UI::Cell::make(layout));
    window->resize({260, 200});
    window->set_resizable(false);
    window->show();
    UI::run();

    return 0;
}