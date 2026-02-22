/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2025-2026 danko1122q */

#include <libui/libui.h>
#include <libui/widget/layout/FlexLayout.h>
#include <libui/widget/layout/BoxLayout.h>
#include <libui/widget/Label.h>
#include <libui/widget/Cell.h>
#include <libui/widget/Image.h>
#include <libsys/Memory.h>
#include <libsys/CPU.h>
#include <libnusa/StringStream.h>
#include <sys/utsname.h>
#include <cstdio>
#include <cstring>

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
        snprintf(buf, sizeof(buf), "%ld day%s, %02ld:%02ld:%02ld",
                 days, days == 1 ? "" : "s", hours, minutes, secs);
    else
        snprintf(buf, sizeof(buf), "%02ld:%02ld:%02ld", hours, minutes, secs);
    return std::string(buf);
}

static void add_info_row(Duck::Ptr<UI::FlexLayout>& layout,
                         const std::string& label,
                         const std::string& value) {
    auto row = UI::BoxLayout::make(UI::BoxLayout::HORIZONTAL, 6);

    auto lbl = UI::Label::make(label, UI::BEGINNING);
    lbl->set_color(RGB(180, 180, 180));
    lbl->set_sizing_mode(UI::PREFERRED);
    row->add_child(lbl);

    auto val = UI::Label::make(value, UI::BEGINNING);
    val->set_color(RGB(180, 180, 180));
    val->set_sizing_mode(UI::FILL);
    row->add_child(val);

    row->set_sizing_mode(UI::PREFERRED);
    layout->add_child(row);
}

int main(int argc, char** argv, char** envp) {
    UI::init(argv, envp);

    utsname uname_buf;
    memset(&uname_buf, 0, sizeof(uname_buf));
    if (uname(&uname_buf))
        exit(-1);

    // Memory
    std::string mem_str = "N/A";
    auto mem_res = Mem::get_info();
    if (!mem_res.is_error()) {
        auto& mem = mem_res.value();
        Duck::StringOutputStream ss;
        ss << mem.used.readable() << " used / " << mem.usable.readable() << " total";
        mem_str = ss.string();
    }

    // CPU
    std::string cpu_str = "N/A";
    Duck::FileInputStream cpu_stream("/proc/cpuinfo");
    auto cpu_res = CPU::get_info(cpu_stream);
    if (!cpu_res.is_error())
        cpu_str = std::to_string(cpu_res.value().utilization) + "% utilization";

    // Uptime
    std::string uptime_str = get_uptime_str();

    auto window = UI::Window::make();
    window->set_title("About " + std::string(uname_buf.sysname));
    window->set_resizable(false);

    auto layout = UI::FlexLayout::make(UI::FlexLayout::VERTICAL);
    layout->set_spacing(6);

    // === Header: icon + nama OS + versi ===
    {
        auto header = UI::BoxLayout::make(UI::BoxLayout::HORIZONTAL, 10);

        // Icon dari resources app
        auto icon = UI::Image::make(UI::app_info().icon());
        header->add_child(icon);

        auto name_col = UI::FlexLayout::make(UI::FlexLayout::VERTICAL);
        name_col->set_spacing(2);

        auto os_name = UI::Label::make(uname_buf.sysname, UI::BEGINNING);
        os_name->set_font(UI::pond_context->get_font("gohu-14"));
        os_name->set_color(RGB(180, 180, 180));
        os_name->set_sizing_mode(UI::PREFERRED);
        name_col->add_child(os_name);

        auto os_ver = UI::Label::make(
            std::string("Version ") + uname_buf.release, UI::BEGINNING);
        os_ver->set_color(RGB(180, 180, 180));
        os_ver->set_sizing_mode(UI::PREFERRED);
        name_col->add_child(os_ver);

        name_col->set_sizing_mode(UI::FILL);
        header->add_child(name_col);
        header->set_sizing_mode(UI::PREFERRED);
        layout->add_child(header);
    }

    // Spacer
    layout->add_child(UI::Label::make(""));

    // === Info rows ===
    add_info_row(layout, "Architecture:", uname_buf.machine);
    add_info_row(layout, "Kernel:      ", uname_buf.release);
    add_info_row(layout, "Hostname:    ", uname_buf.nodename);
    add_info_row(layout, "Uptime:      ", uptime_str);
    add_info_row(layout, "Memory:      ", mem_str);
    add_info_row(layout, "CPU:         ", cpu_str);

    // Spacer
    layout->add_child(UI::Label::make(""));

    // Copyright
    {
        auto copy = UI::Label::make("\xc2\xa9 2025-2026 danko1122q", UI::BEGINNING);
        copy->set_color(RGB(180, 180, 180));
        copy->set_sizing_mode(UI::PREFERRED);
        layout->add_child(copy);
    }

    window->set_contents(UI::Cell::make(layout));
    window->resize({310, 230});
    window->show();

    UI::run();
    return 0;
}