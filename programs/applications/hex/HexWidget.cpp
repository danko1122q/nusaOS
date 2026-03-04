/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 danko1122q */

#include "HexWidget.h"
#include <libui/libui.h>
#include <libui/Theme.h>
#include <libkeyboard/Keyboard.h>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <algorithm>

using namespace UI;
using namespace Gfx;

// Color palette
static const Gfx::Color COL_BG        = {20,  20,  28};
static const Gfx::Color COL_BG_ALT    = {26,  26,  36};
static const Gfx::Color COL_ADDR      = {90,  140, 200};
static const Gfx::Color COL_BYTE_NORM = {200, 200, 210};
static const Gfx::Color COL_BYTE_ZERO = {70,  70,  85};
static const Gfx::Color COL_BYTE_FF   = {220, 100, 80};
static const Gfx::Color COL_ASCII_PRT = {140, 210, 140};
static const Gfx::Color COL_ASCII_NPR = {70,  70,  85};
static const Gfx::Color COL_CURSOR_BG = {60,  120, 200};
static const Gfx::Color COL_CURSOR_FG = {255, 255, 255};
static const Gfx::Color COL_SEL_BG    = {50,  80,  130};
static const Gfx::Color COL_SEP       = {45,  45,  60};
static const Gfx::Color COL_MODIFIED  = {220, 160, 60};
static const Gfx::Color COL_HEADER    = {120, 120, 140};

HexWidget::HexWidget() {
    set_uses_alpha(false);
}

bool HexWidget::load_file(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if(!f) return false;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if(sz < 0) { fclose(f); return false; }

    m_data.resize((size_t)sz);
    if(sz > 0)
        fread(m_data.data(), 1, (size_t)sz, f);
    fclose(f);

    m_file_path   = path;
    m_modified    = false;
    m_cursor      = 0;
    m_high_nibble = true;
    m_ascii_mode  = false;
    m_scroll_row  = 0;
    m_sel_start   = -1;
    m_sel_end     = -1;
    m_total_rows  = (int)((m_data.size() + HEX_BYTES_PER_ROW - 1) / HEX_BYTES_PER_ROW);

    notify_status();
    repaint();
    return true;
}

bool HexWidget::save_file() {
    return save_file_as(m_file_path);
}

bool HexWidget::save_file_as(const std::string& path) {
    if(path.empty()) return false;
    FILE* f = fopen(path.c_str(), "wb");
    if(!f) return false;
    fwrite(m_data.data(), 1, m_data.size(), f);
    fclose(f);
    m_file_path = path;
    m_modified  = false;
    notify_status();
    repaint();
    return true;
}

void HexWidget::set_scroll_row(int row) {
    m_scroll_row = std::max(0, std::min(row, m_total_rows - 1));
    repaint();
}

Gfx::Dimensions HexWidget::preferred_size() {
    int rows = std::max(HEX_VISIBLE_ROWS, m_total_rows);
    return { HEX_TOTAL_WIDTH, HEX_PADDING * 2 + HEX_ROW_HEIGHT + rows * HEX_ROW_HEIGHT };
}

int HexWidget::row_of(size_t offset) const {
    return (int)(offset / HEX_BYTES_PER_ROW);
}

void HexWidget::ensure_cursor_visible() {
    int row = row_of(m_cursor);
    if(row < m_scroll_row)
        m_scroll_row = row;
    else if(row >= m_scroll_row + HEX_VISIBLE_ROWS)
        m_scroll_row = row - HEX_VISIBLE_ROWS + 1;
    m_scroll_row = std::max(0, m_scroll_row);
}

void HexWidget::notify_status() {
    if(on_status_changed)
        on_status_changed();
}

void HexWidget::do_repaint(const DrawContext& ctx) {
    auto font = UI::Theme::font();
    Rect full = ctx.rect();

    // Background
    ctx.fill(full, COL_BG);

    // Column header row
    int header_y = HEX_PADDING;
    ctx.fill({0, header_y - 2, full.width, HEX_ROW_HEIGHT + 2}, COL_BG_ALT);

    // Header: "Offset" label
    ctx.draw_text("Offset", {HEX_PADDING, header_y, HEX_ADDR_WIDTH, HEX_ROW_HEIGHT},
                  BEGINNING, CENTER, font, COL_HEADER);

    // Header: byte column indices 00-0F
    for(int col = 0; col < HEX_BYTES_PER_ROW; col++) {
        char buf[4];
        snprintf(buf, sizeof(buf), "%02X", col);
        int x = HEX_PADDING + HEX_ADDR_WIDTH + col * HEX_BYTE_WIDTH;
        ctx.draw_text(buf, {x, header_y, HEX_BYTE_WIDTH, HEX_ROW_HEIGHT},
                      BEGINNING, CENTER, font, COL_HEADER);
    }

    // Separator line under header
    int sep_y = header_y + HEX_ROW_HEIGHT + 1;
    ctx.fill({0, sep_y, full.width, 1}, COL_SEP);

    // Vertical separator between hex and ascii
    int vsep_x = HEX_PADDING + HEX_ADDR_WIDTH + HEX_BYTES_PER_ROW * HEX_BYTE_WIDTH + HEX_SEP_WIDTH / 2;
    ctx.fill({vsep_x, 0, 1, full.height}, COL_SEP);

    // Data rows
    int data_y_start = sep_y + 2;

    if(m_data.empty()) {
        ctx.draw_text("No file loaded", {HEX_PADDING, data_y_start + 10, 200, HEX_ROW_HEIGHT},
                      BEGINNING, CENTER, font, COL_BYTE_ZERO);
        return;
    }

    int sel_lo = -1, sel_hi = -1;
    if(m_sel_start >= 0 && m_sel_end >= 0) {
        sel_lo = std::min(m_sel_start, m_sel_end);
        sel_hi = std::max(m_sel_start, m_sel_end);
    }

    for(int row = m_scroll_row; row < m_total_rows; row++) {
        int y = data_y_start + (row - m_scroll_row) * HEX_ROW_HEIGHT;
        if(y > full.height) break;

        // Alternate row background
        if(row % 2 == 0)
            ctx.fill({0, y, full.width, HEX_ROW_HEIGHT}, COL_BG);
        else
            ctx.fill({0, y, full.width, HEX_ROW_HEIGHT}, COL_BG_ALT);

        // Address
        size_t row_offset = (size_t)row * HEX_BYTES_PER_ROW;
        char addr_buf[16];
        snprintf(addr_buf, sizeof(addr_buf), "%08zX:", row_offset);
        ctx.draw_text(addr_buf, {HEX_PADDING, y, HEX_ADDR_WIDTH, HEX_ROW_HEIGHT},
                      BEGINNING, CENTER, font, COL_ADDR);

        // Bytes
        for(int col = 0; col < HEX_BYTES_PER_ROW; col++) {
            size_t offset = row_offset + (size_t)col;
            if(offset >= m_data.size()) break;

            uint8_t byte = m_data[offset];
            int bx = HEX_PADDING + HEX_ADDR_WIDTH + col * HEX_BYTE_WIDTH;

            // Selection highlight
            bool in_sel = (sel_lo >= 0 && (int)offset >= sel_lo && (int)offset <= sel_hi);
            bool is_cursor = (offset == m_cursor);

            if(is_cursor && !m_ascii_mode) {
                ctx.fill({bx - 1, y, HEX_BYTE_WIDTH - 1, HEX_ROW_HEIGHT}, COL_CURSOR_BG);
            } else if(in_sel) {
                ctx.fill({bx - 1, y, HEX_BYTE_WIDTH - 1, HEX_ROW_HEIGHT}, COL_SEL_BG);
            }

            // Byte color by value
            Gfx::Color byte_col;
            if(byte == 0x00)        byte_col = COL_BYTE_ZERO;
            else if(byte == 0xFF)   byte_col = COL_BYTE_FF;
            else                    byte_col = COL_BYTE_NORM;

            if(is_cursor && !m_ascii_mode) byte_col = COL_CURSOR_FG;
            else if(in_sel)                byte_col = COL_CURSOR_FG;

            char hex_buf[4];
            snprintf(hex_buf, sizeof(hex_buf), "%02X", byte);

            // Modified indicator: show high nibble cursor
            if(is_cursor && !m_ascii_mode && !m_high_nibble) {
                // dim first char to show we're on low nibble
                char hi[2] = {hex_buf[0], 0};
                char lo[2] = {hex_buf[1], 0};
                ctx.draw_text(hi, {bx, y, 7, HEX_ROW_HEIGHT}, BEGINNING, CENTER, font, {180,180,180});
                ctx.draw_text(lo, {bx+7, y, 7, HEX_ROW_HEIGHT}, BEGINNING, CENTER, font, COL_CURSOR_FG);
            } else {
                ctx.draw_text(hex_buf, {bx, y, HEX_BYTE_WIDTH, HEX_ROW_HEIGHT},
                              BEGINNING, CENTER, font, byte_col);
            }

            // ASCII side
            int ax = HEX_PADDING + HEX_ADDR_WIDTH + HEX_BYTES_PER_ROW * HEX_BYTE_WIDTH
                     + HEX_SEP_WIDTH + col * HEX_ASCII_WIDTH;

            bool ascii_cursor = (offset == m_cursor && m_ascii_mode);
            if(ascii_cursor)
                ctx.fill({ax, y, HEX_ASCII_WIDTH - 1, HEX_ROW_HEIGHT}, COL_CURSOR_BG);
            else if(in_sel)
                ctx.fill({ax, y, HEX_ASCII_WIDTH - 1, HEX_ROW_HEIGHT}, COL_SEL_BG);

            char asc_buf[2] = { (char)(isprint(byte) ? byte : '.'), 0 };
            Gfx::Color asc_col = isprint(byte) ? COL_ASCII_PRT : COL_ASCII_NPR;
            if(ascii_cursor || in_sel) asc_col = COL_CURSOR_FG;

            ctx.draw_text(asc_buf, {ax, y, HEX_ASCII_WIDTH, HEX_ROW_HEIGHT},
                          BEGINNING, CENTER, font, asc_col);
        }
    }
}

int HexWidget::pos_to_offset(Gfx::Point pos) const {
    int sep_y    = HEX_PADDING + HEX_ROW_HEIGHT + 3;
    int data_y   = sep_y + 2;

    if(pos.y < data_y) return -1;

    int row = m_scroll_row + (pos.y - data_y) / HEX_ROW_HEIGHT;
    if(row < 0 || row >= m_total_rows) return -1;

    // Check hex area
    int hex_x_start = HEX_PADDING + HEX_ADDR_WIDTH;
    int hex_x_end   = hex_x_start + HEX_BYTES_PER_ROW * HEX_BYTE_WIDTH;
    if(pos.x >= hex_x_start && pos.x < hex_x_end) {
        int col = (pos.x - hex_x_start) / HEX_BYTE_WIDTH;
        if(col < 0 || col >= HEX_BYTES_PER_ROW) return -1;
        int offset = row * HEX_BYTES_PER_ROW + col;
        if(offset >= (int)m_data.size()) return -1;
        m_ascii_mode = false;
        return offset;
    }

    // Check ascii area
    int asc_x_start = hex_x_end + HEX_SEP_WIDTH;
    int asc_x_end   = asc_x_start + HEX_BYTES_PER_ROW * HEX_ASCII_WIDTH;
    if(pos.x >= asc_x_start && pos.x < asc_x_end) {
        int col = (pos.x - asc_x_start) / HEX_ASCII_WIDTH;
        if(col < 0 || col >= HEX_BYTES_PER_ROW) return -1;
        int offset = row * HEX_BYTES_PER_ROW + col;
        if(offset >= (int)m_data.size()) return -1;
        m_ascii_mode = true;
        return offset;
    }

    return -1;
}

bool HexWidget::on_mouse_button(Pond::MouseButtonEvent evt) {
    if(!(evt.old_buttons & POND_MOUSE1) && (evt.new_buttons & POND_MOUSE1)) {
        auto pos = mouse_position();
        int off = pos_to_offset(pos);
        if(off >= 0) {
            m_cursor      = (size_t)off;
            m_high_nibble = true;
            m_sel_start   = off;
            m_sel_end     = off;
            m_selecting   = true;
            notify_status();
            repaint();
        }
        return true;
    }
    if((evt.old_buttons & POND_MOUSE1) && !(evt.new_buttons & POND_MOUSE1)) {
        m_selecting = false;
    }
    return false;
}

bool HexWidget::on_mouse_move(Pond::MouseMoveEvent evt) {
    if(m_selecting) {
        int off = pos_to_offset(evt.new_pos);
        if(off >= 0 && off != m_sel_end) {
            m_sel_end = off;
            repaint();
        }
    }
    return false;
}

bool HexWidget::on_mouse_scroll(Pond::MouseScrollEvent evt) {
    m_scroll_row = std::max(0, std::min(m_scroll_row - evt.scroll * 3, m_total_rows - 1));
    repaint();
    return true;
}

void HexWidget::handle_hex_input(char c) {
    if(m_cursor >= m_data.size()) return;
    c = (char)toupper(c);
    int nibble = -1;
    if(c >= '0' && c <= '9') nibble = c - '0';
    else if(c >= 'A' && c <= 'F') nibble = c - 'A' + 10;
    if(nibble < 0) return;

    if(m_high_nibble) {
        m_data[m_cursor] = (m_data[m_cursor] & 0x0F) | (uint8_t)(nibble << 4);
        m_high_nibble = false;
    } else {
        m_data[m_cursor] = (m_data[m_cursor] & 0xF0) | (uint8_t)nibble;
        m_high_nibble = true;
        if(m_cursor + 1 < m_data.size())
            m_cursor++;
    }
    m_modified = true;
    notify_status();
    ensure_cursor_visible();
    repaint();
}

void HexWidget::handle_ascii_input(char c) {
    if(m_cursor >= m_data.size()) return;
    m_data[m_cursor] = (uint8_t)c;
    m_modified = true;
    if(m_cursor + 1 < m_data.size())
        m_cursor++;
    notify_status();
    ensure_cursor_visible();
    repaint();
}

bool HexWidget::on_keyboard(Pond::KeyEvent evt) {
    if(m_data.empty()) return false;
    if(!KBD_ISPRESSED(evt)) return false;

    // Navigation
    switch((Keyboard::Key) evt.key) {
        case Keyboard::Left:
            if(m_cursor > 0) { m_cursor--; m_high_nibble = true; }
            ensure_cursor_visible(); notify_status(); repaint(); return true;
        case Keyboard::Right:
            if(m_cursor + 1 < m_data.size()) { m_cursor++; m_high_nibble = true; }
            ensure_cursor_visible(); notify_status(); repaint(); return true;
        case Keyboard::Up:
            if(m_cursor >= HEX_BYTES_PER_ROW) { m_cursor -= HEX_BYTES_PER_ROW; m_high_nibble = true; }
            ensure_cursor_visible(); notify_status(); repaint(); return true;
        case Keyboard::Down:
            if(m_cursor + HEX_BYTES_PER_ROW < m_data.size()) { m_cursor += HEX_BYTES_PER_ROW; m_high_nibble = true; }
            ensure_cursor_visible(); notify_status(); repaint(); return true;
        case Keyboard::Tab:
            m_ascii_mode = !m_ascii_mode;
            m_high_nibble = true;
            repaint(); return true;
        default: break;
    }

    // Editing
    if(!m_ascii_mode) {
        char c = (char)evt.character;
        if((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
            handle_hex_input(c);
            return true;
        }
    } else {
        if(evt.character >= 0x20 && evt.character < 0x7F) {
            handle_ascii_input((char)evt.character);
            return true;
        }
    }

    return false;
}