/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2025-2026 danko1122q */

#include "CpuGraphWidget.h"

void CpuGraphWidget::update() {
    Duck::FileInputStream stream("/proc/cpuinfo");
    auto res = Sys::CPU::get_info(stream);
    if (res.has_value())
        m_util = res.value().utilization;

    float val = (float)m_util / 100.0f;
    if (val < 0.0f) val = 0.0f;
    if (val > 1.0f) val = 1.0f;

    // Push baru di belakang, buang terlama dari depan
    // index 0 = terlama (kiri), index n-1 = terbaru (kanan)
    m_values.push_back(val);
    while ((int)m_values.size() > 200)
        m_values.erase(m_values.begin());

    repaint();
}

void CpuGraphWidget::do_repaint(const UI::DrawContext& ctx) {
    ctx.fill(ctx.rect(), Gfx::Color(0, 0, 0));
    ctx.draw_inset_outline(ctx.rect());

    int w = ctx.width() - 4;   // lebar area grafik
    int h = ctx.height() - 4;  // tinggi area grafik
    int n = (int)m_values.size();
    Gfx::Color bar(220, 60, 60);

    // Gambar dari x=2 (kiri) ke kanan
    // Data terlama (index 0) di kiri, terbaru (index n-1) di kanan
    // Jika data < w, mulai dari kiri (x=2), sisanya kosong di kanan
    for (int i = 0; i < n && i < w; i++) {
        int x  = 2 + i;
        int bh = (int)(m_values[i] * h);
        if (bh < 1) bh = 1;
        if (bh > h) bh = h;
        ctx.fill({x, ctx.height() - 2 - bh, 1, bh}, bar);
    }

    std::string lbl = std::to_string(m_util) + "%";
    ctx.draw_text(lbl.c_str(), ctx.rect(),
        UI::CENTER, UI::CENTER,
        UI::Theme::font(), Gfx::Color(255, 255, 255));
}

Gfx::Dimensions CpuGraphWidget::preferred_size() {
    return {200, 50};
}

void CpuGraphWidget::on_layout_change(const Gfx::Rect&) {}