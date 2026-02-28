/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2026 nusaOS */

#pragma once

#include <libui/widget/Widget.h>
#include <libsys/Memory.h>
#include <libnusa/FileStream.h>
#include <vector>

class MemGraphWidget: public UI::Widget {
public:
    WIDGET_DEF(MemGraphWidget)

    void update();
    Gfx::Dimensions preferred_size() override;

protected:
    void do_repaint(const UI::DrawContext& ctx) override;
    void on_layout_change(const Gfx::Rect& old_rect) override;

private:
    MemGraphWidget() = default;
    std::vector<float> m_values;
    Sys::Mem::Info m_last_info = {};
};