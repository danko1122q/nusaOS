/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 danko1122q */

#pragma once

#include <libui/widget/Widget.h>
#include <libui/widget/ScrollView.h>
#include <libpond/Event.h>
#include <vector>
#include <string>
#include <functional>

// Layout constants
#define HEX_BYTES_PER_ROW    16
#define HEX_ROW_HEIGHT       14
#define HEX_ADDR_WIDTH       72   // "0x00000000: "
#define HEX_BYTE_WIDTH       22   // "FF " per byte
#define HEX_SEP_WIDTH         8   // gap between hex and ascii
#define HEX_ASCII_WIDTH      10   // per ascii char
#define HEX_PADDING           6
#define HEX_VISIBLE_ROWS     32
#define HEX_TOTAL_WIDTH      (HEX_ADDR_WIDTH + HEX_BYTES_PER_ROW * HEX_BYTE_WIDTH + HEX_SEP_WIDTH + HEX_BYTES_PER_ROW * HEX_ASCII_WIDTH + HEX_PADDING * 2)

class HexWidget : public UI::Widget {
public:
	WIDGET_DEF(HexWidget);

	// Load data from file path
	bool load_file(const std::string& path);

	// Save data back to file
	bool save_file();
	bool save_file_as(const std::string& path);

	// Returns current file path
	const std::string& file_path() const { return m_file_path; }

	// Returns whether there are unsaved changes
	bool is_modified() const { return m_modified; }

	// Total number of bytes loaded
	size_t file_size() const { return m_data.size(); }

	// Cursor offset
	size_t cursor() const { return m_cursor; }

	// Scroll offset (in rows)
	int scroll_row() const { return m_scroll_row; }
	void set_scroll_row(int row);

	// Called whenever cursor/modified/selection changes
	std::function<void()> on_status_changed;

	Gfx::Dimensions preferred_size() override;

protected:
	void do_repaint(const UI::DrawContext& ctx) override;
	bool on_mouse_button(Pond::MouseButtonEvent evt) override;
	bool on_mouse_move(Pond::MouseMoveEvent evt) override;
	bool on_keyboard(Pond::KeyEvent evt) override;
	bool on_mouse_scroll(Pond::MouseScrollEvent evt) override;

private:
	HexWidget();

	// Convert pixel position to byte offset (-1 if not on a byte)
	int pos_to_offset(Gfx::Point pos) const;

	// Row for a given offset
	int row_of(size_t offset) const;

	// Ensure cursor is visible (scroll if needed)
	void ensure_cursor_visible();

	// Handle hex nibble input
	void handle_hex_input(char c);

	// Handle ascii input
	void handle_ascii_input(char c);

	// Notify status change
	void notify_status();

	std::vector<uint8_t> m_data;
	std::string          m_file_path;
	bool                 m_modified    = false;

	size_t               m_cursor      = 0;   // byte offset of cursor
	bool                 m_high_nibble = true; // true = editing high nibble
	mutable bool         m_ascii_mode  = false;// editing ascii side

	int                  m_scroll_row  = 0;
	int                  m_total_rows  = 0;

	// Selection
	int                  m_sel_start   = -1;
	int                  m_sel_end     = -1;
	bool                 m_selecting   = false;
};