/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 danko1122q */

#include "HexEditor.h"
#include <libui/libui.h>
#include <libui/widget/layout/BoxLayout.h>
#include <libui/widget/Cell.h>
#include <libui/widget/MenuBar.h>
#include <libui/Menu.h>
#include <libui/bits/FilePicker.h>

using namespace UI;
using namespace Duck;

HexEditor::HexEditor() {
	m_window = UI::Window::make();
	m_window->set_title("Hex Editor");
	m_window->set_resizable(true);

	// Root layout: menubar + content + statusbar
	auto root = UI::BoxLayout::make(UI::BoxLayout::VERTICAL, 0);

	// Build File menu
	auto file_menu = UI::Menu::make();

	file_menu->add_item(UI::MenuItem::make("Open...", [this]() {
		auto picker = UI::FilePicker::make(UI::FilePicker::OPEN_SINGLE, Duck::Path("/"));
		auto picked = picker->pick();
		if(!picked.empty())
			open_file(picked[0].string());
	}));

	file_menu->add_item(UI::MenuItem::make("Save", [this]() {
		if(m_hex_widget->file_path().empty()) return;
		m_hex_widget->save_file();
	}));

	file_menu->add_item(UI::MenuItem::make("Save As...", [this]() {
		auto picker = UI::FilePicker::make(UI::FilePicker::SAVE,
		    m_hex_widget->file_path().empty() ? Duck::Path("/") : Duck::Path(m_hex_widget->file_path()));
		auto picked = picker->pick();
		if(!picked.empty())
			m_hex_widget->save_file_as(picked[0].string());
	}));

	file_menu->add_item(UI::MenuItem::Separator);

	file_menu->add_item(UI::MenuItem::make("Close", [this]() {
		m_window->close();
	}));

	// MenuBar takes a top-level Menu whose items become the bar buttons
	auto top_menu = UI::Menu::make();
	top_menu->add_item(UI::MenuItem::make("File", file_menu));

	auto menubar = UI::MenuBar::make(top_menu);
	root->add_child(menubar);

	// HexWidget inside a Cell that fills remaining space
	m_hex_widget = HexWidget::make();
	auto content_cell = UI::Cell::make(m_hex_widget, 0);
	content_cell->set_sizing_mode(UI::FILL);
	root->add_child(content_cell);

	// StatusBar at bottom
	m_status_bar = StatusBar::make();
	root->add_child(m_status_bar);

	// Wire status updates
	m_hex_widget->on_status_changed = [this]() {
		m_status_bar->set_info(
			m_hex_widget->cursor(),
			m_hex_widget->file_size(),
			m_hex_widget->is_modified(),
			m_hex_widget->file_path()
		);
		std::string title = "Hex Editor";
		if(!m_hex_widget->file_path().empty()) {
			auto& p = m_hex_widget->file_path();
			size_t slash = p.rfind('/');
			std::string fname = (slash != std::string::npos) ? p.substr(slash + 1) : p;
			title = fname + (m_hex_widget->is_modified() ? " * — Hex Editor" : " — Hex Editor");
		}
		m_window->set_title(title);
	};

	m_window->set_contents(root);
	m_window->resize({700, 520});
	m_window->show();
}

void HexEditor::open_file(const std::string& path) {
	if(path.empty()) return;
	if(!m_hex_widget->load_file(path)) return;
	m_hex_widget->on_status_changed();
}