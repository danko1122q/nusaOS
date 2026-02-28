/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2026 nusaOS */

#include "MemGraphWidget.h"
#include <algorithm>

void MemGraphWidget::update() {
    Duck::FileInputStream stream("/proc/meminfo");
    auto res = Sys::Mem::get_info(stream);
    float val = 0.0f;
    if (res.has_value()) {
        m_last_info = res.value();
        if (m_last_info.usable)
            val = (float)m_last_info.used / (float)m_last_info.usable;
    }
    val = std::max(0.0f, std::min(1.0f, val));

    if (!m_values.empty())
        m_values.erase(m_values.end() - 1);
    m_values.insert(m_values.begin(), val);
    repaint();
}

void MemGraphWidget::do_repaint(const UI::DrawContext& ctx) {
    ctx.draw_inset_rect(ctx.rect(),
        Gfx::Color(0,0,0),
        UI::Theme::shadow_1(),
        UI::Theme::shadow_2(),
        UI::Theme::highlight());

    int max_h = ctx.height() - 3;
    // Warna biru-hijau untuk RAM — beda dari CPU supaya mudah dibedakan
    Gfx::Color color = UI::Theme::accent();

    for (int x = 0; x < ctx.width() - 3; x++) {
        if (x >= (int)m_values.size()) break;
        int bar_h = std::min(std::max((int)(m_values[x] * max_h), 1), max_h);
        ctx.fill({x + 2, ctx.height() - 1 - bar_h, 1, bar_h}, color);
    }

    // Label teks used/total di tengah grafik
    std::string label = "RAM: " + m_last_info.used.readable()
                      + " / " + m_last_info.usable.readable();
    ctx.draw_text(label.c_str(), ctx.rect(),
        UI::TextAlignment::CENTER, UI::TextAlignment::CENTER,
        UI::Theme::font(), UI::Theme::fg());
}

Gfx::Dimensions MemGraphWidget::preferred_size() {
    return {160, 40};
}

void MemGraphWidget::on_layout_change(const Gfx::Rect& old_rect) {
    m_values.resize(std::max(current_rect().width - 2, 1));
}