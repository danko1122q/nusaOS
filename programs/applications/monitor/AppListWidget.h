/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2026 nusaOS */

#pragma once

#include <libui/widget/Widget.h>
#include <libapp/App.h>
#include <vector>
#include <string>
#include <map>
#include <set>

struct RunningApp {
	std::string name;
	int pid;
};

class AppListWidget: public UI::Widget {
public:
	WIDGET_DEF(AppListWidget)

	void update();
	Gfx::Dimensions preferred_size() override;

protected:
	void initialize() override {}
	void do_repaint(const UI::DrawContext& ctx) override;

private:
	AppListWidget() = default;

	std::vector<RunningApp> m_apps;
};