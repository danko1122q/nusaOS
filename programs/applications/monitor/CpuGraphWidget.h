/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2026 nusaOS */

#pragma once

#include <libui/widget/Widget.h>
#include <libsys/CPU.h>
#include <libnusa/FileStream.h>
#include <vector>

// Widget grafik CPU — clone pola GraphModule dari sandbar,
// tapi standalone (tidak butuh Sandbar.h / Sandbar::HEIGHT).
class CpuGraphWidget: public UI::Widget {
public:
    WIDGET_DEF(CpuGraphWidget)

    void update();
    Gfx::Dimensions preferred_size() override;

protected:
    void do_repaint(const UI::DrawContext& ctx) override;
    void on_layout_change(const Gfx::Rect& old_rect) override;

private:
    CpuGraphWidget() = default;
    std::vector<float> m_values;
    int m_last_util = 0;
};