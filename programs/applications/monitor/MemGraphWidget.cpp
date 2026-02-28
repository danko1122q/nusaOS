/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2025-2026 danko1122q */


#include "MemGraphWidget.h"

void MemGraphWidget::update() {
    Duck::FileInputStream stream("/proc/meminfo");
    auto res = Sys::Mem::get_info(stream);
    if (res.has_value()) {
        auto& info = res.value();
        float frac = (float)info.used_frac();
        if (frac < 0.0f) frac = 0.0f;
        if (frac > 1.0f) frac = 1.0f;
        m_frac  = frac;
        m_label = info.used.readable() + " / " + info.usable.readable();
    }

    m_values.push_back(m_frac);
    while ((int)m_values.size() > 200)
        m_values.erase(m_values.begin());

    repaint();
}

void MemGraphWidget::do_repaint(const UI::DrawContext& ctx) {
    ctx.fill(ctx.rect(), Gfx::Color(0, 0, 0));
    ctx.draw_inset_outline(ctx.rect());

    int w = ctx.width() - 4;
    int h = ctx.height() - 4;
    int n = (int)m_values.size();
    Gfx::Color bar = UI::Theme::accent();

    // Sama: index 0 di kiri, tumbuh ke kanan
    for (int i = 0; i < n && i < w; i++) {
        int x  = 2 + i;
        int bh = (int)(m_values[i] * h);
        if (bh < 1) bh = 1;
        if (bh > h) bh = h;
        ctx.fill({x, ctx.height() - 2 - bh, 1, bh}, bar);
    }

    ctx.draw_text(m_label.c_str(), ctx.rect(),
        UI::CENTER, UI::CENTER,
        UI::Theme::font(), Gfx::Color(255, 255, 255));
}

Gfx::Dimensions MemGraphWidget::preferred_size() {
    return {200, 50};
}

void MemGraphWidget::on_layout_change(const Gfx::Rect&) {}