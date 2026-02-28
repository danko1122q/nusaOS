/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2026 nusaOS */

#pragma once

#include <libui/widget/TableView.h>
#include <libsys/Process.h>

class ProcessListWidget: public UI::Widget, public UI::TableViewDelegate {
public:
	WIDGET_DEF(ProcessListWidget);
	void update();

protected:
	Duck::Ptr<Widget> tv_create_entry(int row, int col) override;
	std::string tv_column_name(int col) override;
	int tv_num_entries() override;
	int tv_row_height() override;
	int tv_column_width(int col) override;
	UI::TableViewSelectionMode tv_selection_mode() override { return UI::TableViewSelectionMode::SINGLE; }
	void tv_selection_changed(const std::set<int>& selected_items) override;
	Duck::Ptr<UI::Menu> tv_entry_menu(int row) override;
	void initialize() override;

private:
	// Kolom: PID | Name | Virtual | Physical | Shared | State
	enum Col { PID=0, Name=1, Virtual=2, Physical=3, Shared=4, State=5, _COUNT=6 };

	ProcessListWidget();
	std::vector<Sys::Process> m_processes;
	Duck::Ptr<UI::TableView> m_table_view = UI::TableView::make(Col::_COUNT, true);
};