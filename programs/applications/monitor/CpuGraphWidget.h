/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2025-2026 danko1122q */

#pragma once

#include <libui/widget/Widget.h>
#include <libsys/CPU.h>
#include <libnusa/FileStream.h>
#include <vector>
#include <string>

class CpuGraphWidget: public UI::Widget {
public:
    WIDGET_DEF(CpuGraphWidget)

    void update();
    Gfx::Dimensions preferred_size() override;

protected:
    void initialize() override {}
    void do_repaint(const UI::DrawContext& ctx) override;
    void on_layout_change(const Gfx::Rect& old_rect) override;

private:
    CpuGraphWidget() = default;

    std::vector<float> m_values;
    int m_util = 0;
};