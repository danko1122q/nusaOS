/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2026 nusaOS */

#include "CpuGraphWidget.h"
#include <algorithm>

void CpuGraphWidget::update() {
    // Buka stream baru tiap kali — /proc tidak bisa rewind
    Duck::FileInputStream stream("/proc/cpuinfo");
    auto res = Sys::CPU::get_info(stream);
    float val = 0.0f;
    if (res.has_value()) {
        m_last_util = res.value().utilization;
        val = m_last_util / 100.0f;
    }

    // Geser nilai lama ke kanan, masukkan nilai baru di depan
    if (!m_values.empty())
        m_values.erase(m_values.end() - 1);
    m_values.insert(m_values.begin(), val);
    repaint();
}

void CpuGraphWidget::do_repaint(const UI::DrawContext& ctx) {
    // Background + border inset (sama persis dengan GraphModule sandbar)
    ctx.draw_inset_rect(ctx.rect(),
        Gfx::Color(0,0,0),
        UI::Theme::shadow_1(),
        UI::Theme::shadow_2(),
        UI::Theme::highlight());

    int max_h = ctx.height() - 3;
    Gfx::Color color(220, 50, 50); // merah — sama dengan CPUModule sandbar

    for (int x = 0; x < ctx.width() - 3; x++) {
        if (x >= (int)m_values.size()) break;
        int bar_h = std::min(std::max((int)(m_values[x] * max_h), 1), max_h);
        ctx.fill({x + 2, ctx.height() - 1 - bar_h, 1, bar_h}, color);
    }

    // Label teks % di tengah grafik
    std::string label = "CPU: " + std::to_string(m_last_util) + "%";
    ctx.draw_text(label.c_str(), ctx.rect(),
        UI::TextAlignment::CENTER, UI::TextAlignment::CENTER,
        UI::Theme::font(), UI::Theme::fg());
}

Gfx::Dimensions CpuGraphWidget::preferred_size() {
    return {120, 40};
}

void CpuGraphWidget::on_layout_change(const Gfx::Rect& old_rect) {
    m_values.resize(std::max(current_rect().width - 2, 1));
}