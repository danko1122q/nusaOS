/*
	This file is part of nusaOS.
	SPDX-License-Identifier: GPL-3.0-or-later
	Copyright (c) danko1122q 2026. All rights reserved.
*/

#pragma once

#include <libui/widget/Widget.h>
#include <libui/Timer.h>
#include <libgraphics/Graphics.h>
#include <cstdint>
#include <cstring>

// Layout
#define BLOCK_SIZE       20
#define FIELD_PADDING    10
#define FIELD_WIDTH      10
#define FIELD_HEIGHT     20
#define FIELD_LEFT       FIELD_PADDING
#define FIELD_TOP        FIELD_PADDING
#define FIELD_RIGHT      (FIELD_PADDING + BLOCK_SIZE * FIELD_WIDTH)
#define FIELD_BOTTOM     (FIELD_PADDING + BLOCK_SIZE * FIELD_HEIGHT)

#define SIDE_W           140
#define SIDE_LEFT        (FIELD_RIGHT + FIELD_PADDING)
#define SIDE_TOP         FIELD_TOP
#define SIDE_RIGHT       (SIDE_LEFT + SIDE_W)
#define SIDE_BOTTOM      FIELD_BOTTOM

#define TETRIS_W         (FIELD_RIGHT + FIELD_PADDING + SIDE_W + FIELD_PADDING)
#define TETRIS_H         (FIELD_BOTTOM + FIELD_PADDING)

// Kecepatan timer (ms)
#define TICK_MS          800
#define DROP_TICK_MS     60
#define CLEAR_TICK_MS    60
#define START_TICK_MS    600

// Piece shape
#define PIECE_W          4
#define PIECE_H          4
#define PIECE_COUNT      7

// Block type 
enum Block : uint8_t {
	BLK_NONE = 0,
	BLK_CYAN,
	BLK_BLUE,
	BLK_ORANGE,
	BLK_YELLOW,
	BLK_GREEN,
	BLK_PURPLE,
	BLK_RED,
	BLK_CLEARING,
	BLK_OUTLINE,
	BLK_COUNT
};

using Piece = Block[PIECE_H][PIECE_W];

class TetrisWidget : public UI::Widget {
public:
	WIDGET_DEF(TetrisWidget)

	Gfx::Dimensions preferred_size() override;
	void do_repaint(const UI::DrawContext& ctx) override;
	bool on_keyboard(Pond::KeyEvent evt) override;
	bool on_mouse_button(Pond::MouseButtonEvent evt) override;

protected:
	void initialize() override;
	void on_layout_change(const Gfx::Rect& old_rect) override;

private:
	TetrisWidget();

	// State
	enum class State { START, PLAYING, CLEARING, GAMEOVER };
	State   m_state = State::START;
	bool    m_blink = false;
	int     m_clear_ticks = 0;  // counter tick animasi clearing

	Block   m_field[FIELD_HEIGHT][FIELD_WIDTH] = {};

	Piece   m_cur_piece = {};
	int     m_cur_x = 0, m_cur_y = 0;
	bool    m_dropping = false;

	uint64_t m_score = 0, m_lines = 0, m_pieces = 0;
	uint64_t m_old_score = 0, m_old_lines = 0, m_old_pieces = 0;

	Duck::Ptr<UI::Timer> m_timer;

	// Piece definitions 
	static const Piece s_pieces[PIECE_COUNT];

	// Colors
	static const Gfx::Color s_normal[BLK_COUNT];
	static const Gfx::Color s_highlight[BLK_COUNT];
	static const Gfx::Color s_shadow[BLK_COUNT];

	// Helpers 
	void game_start();
	void game_pause();   // reset to start screen

	void tick();         // called by timer

	// Piece logic
	void   piece_new();
	void   piece_rotate(Piece& p);
	bool   piece_oob(const Piece& p, int px, int py) const;
	bool   piece_collides(const Piece& p, int px, int py) const;
	void   piece_commit();    // bake current piece to field
	void   piece_drop_instant();

	// Field logic
	void   field_check_lines();
	void   field_clear_tick();   // animasi clearing satu frame
	void   field_move_down(int line);

	// Draw helpers (hanya pakai ctx.fill / fill_ellipse / draw_text)
	void draw_block(const UI::DrawContext& ctx, Block b, int fx, int fy) const;
	void draw_field(const UI::DrawContext& ctx);
	void draw_field_border(const UI::DrawContext& ctx) const;
	void draw_side_panel(const UI::DrawContext& ctx) const;
	void draw_current_piece(const UI::DrawContext& ctx) const;
	void draw_start_screen(const UI::DrawContext& ctx) const;
	void draw_gameover(const UI::DrawContext& ctx) const;
	void draw_stat(const UI::DrawContext& ctx, int y_offset,
	               const char* label, uint64_t val) const;

	// Outline (ghost piece) Y
	int ghost_y() const;
};