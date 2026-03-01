/*
 * nusaOS Tetris Port
 * ==================
 * This file is part of nusaOS.
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) danko1122q 2026. All rights reserved.
 *
 * CREDITS:
 * Original logic and implementation by Kai Norberg (PatchworkOS).
 * Source: https://github.com/KaiNorberg/PatchworkOS
 *
 */

#include "TetrisWidget.h"
#include <libui/libui.h>
#include <libpond/pond.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>

// ─── Piece definitions (sama persis dengan tetris.c) ─────────────────────────
const Piece TetrisWidget::s_pieces[PIECE_COUNT] = {
	// CYAN  — I
	{ {BLK_NONE,BLK_NONE,BLK_NONE,BLK_NONE},
	  {BLK_NONE,BLK_NONE,BLK_NONE,BLK_NONE},
	  {BLK_CYAN,BLK_CYAN,BLK_CYAN,BLK_CYAN},
	  {BLK_NONE,BLK_NONE,BLK_NONE,BLK_NONE} },
	// BLUE  — J
	{ {BLK_NONE,BLK_NONE,BLK_NONE,BLK_NONE},
	  {BLK_BLUE,BLK_NONE,BLK_NONE,BLK_NONE},
	  {BLK_BLUE,BLK_BLUE,BLK_BLUE,BLK_NONE},
	  {BLK_NONE,BLK_NONE,BLK_NONE,BLK_NONE} },
	// ORANGE — L
	{ {BLK_NONE,  BLK_NONE,  BLK_NONE,  BLK_NONE},
	  {BLK_NONE,  BLK_NONE,  BLK_ORANGE,BLK_NONE},
	  {BLK_ORANGE,BLK_ORANGE,BLK_ORANGE,BLK_NONE},
	  {BLK_NONE,  BLK_NONE,  BLK_NONE,  BLK_NONE} },
	// YELLOW — O
	{ {BLK_NONE,  BLK_NONE,  BLK_NONE,  BLK_NONE},
	  {BLK_NONE,  BLK_YELLOW,BLK_YELLOW,BLK_NONE},
	  {BLK_NONE,  BLK_YELLOW,BLK_YELLOW,BLK_NONE},
	  {BLK_NONE,  BLK_NONE,  BLK_NONE,  BLK_NONE} },
	// GREEN — S
	{ {BLK_NONE, BLK_NONE, BLK_NONE,BLK_NONE},
	  {BLK_NONE, BLK_GREEN,BLK_GREEN,BLK_NONE},
	  {BLK_GREEN,BLK_GREEN,BLK_NONE, BLK_NONE},
	  {BLK_NONE, BLK_NONE, BLK_NONE,BLK_NONE} },
	// PURPLE — T
	{ {BLK_NONE,  BLK_NONE,  BLK_NONE,  BLK_NONE},
	  {BLK_NONE,  BLK_PURPLE,BLK_NONE,  BLK_NONE},
	  {BLK_PURPLE,BLK_PURPLE,BLK_PURPLE,BLK_NONE},
	  {BLK_NONE,  BLK_NONE,  BLK_NONE,  BLK_NONE} },
	// RED — Z
	{ {BLK_NONE,BLK_NONE,BLK_NONE,BLK_NONE},
	  {BLK_RED, BLK_RED, BLK_NONE,BLK_NONE},
	  {BLK_NONE,BLK_RED, BLK_RED, BLK_NONE},
	  {BLK_NONE,BLK_NONE,BLK_NONE,BLK_NONE} },
};

// ─── Colors ───────────────────────────────────────────────────────────────────
const Gfx::Color TetrisWidget::s_normal[BLK_COUNT] = {
	Gfx::Color(0x00,0x00,0x00), // NONE
	Gfx::Color(0x00,0xE5,0xFF), // CYAN
	Gfx::Color(0x00,0x55,0xFF), // BLUE
	Gfx::Color(0xFF,0x7A,0x00), // ORANGE
	Gfx::Color(0xFF,0xE1,0x00), // YELLOW
	Gfx::Color(0x00,0xFF,0x4D), // GREEN
	Gfx::Color(0xD2,0x00,0xFF), // PURPLE
	Gfx::Color(0xFF,0x00,0x55), // RED
	Gfx::Color(0xFF,0xFF,0xFF), // CLEARING
	Gfx::Color(0x22,0x22,0x22), // OUTLINE
};
const Gfx::Color TetrisWidget::s_highlight[BLK_COUNT] = {
	Gfx::Color(0x00,0x00,0x00),
	Gfx::Color(0x98,0xF5,0xFF),
	Gfx::Color(0x98,0xB9,0xFF),
	Gfx::Color(0xFF,0xBF,0x98),
	Gfx::Color(0xFF,0xF3,0x98),
	Gfx::Color(0x98,0xFF,0xB3),
	Gfx::Color(0xED,0x98,0xFF),
	Gfx::Color(0xFF,0x98,0xB9),
	Gfx::Color(0xFF,0xFF,0xFF),
	Gfx::Color(0x66,0x66,0x66),
};
const Gfx::Color TetrisWidget::s_shadow[BLK_COUNT] = {
	Gfx::Color(0x00,0x00,0x00),
	Gfx::Color(0x00,0x7A,0x8C),
	Gfx::Color(0x00,0x2A,0x8C),
	Gfx::Color(0x8C,0x46,0x00),
	Gfx::Color(0x8C,0x7D,0x00),
	Gfx::Color(0x00,0x8C,0x2A),
	Gfx::Color(0x75,0x00,0x8C),
	Gfx::Color(0x8C,0x00,0x2A),
	Gfx::Color(0xFF,0xFF,0xFF),
	Gfx::Color(0x11,0x11,0x11),
};

// ─── Constructor / initialize ─────────────────────────────────────────────────

TetrisWidget::TetrisWidget() {
	srand((unsigned)time(nullptr));
}

void TetrisWidget::initialize() {
	game_pause(); // reset ke start screen
	m_timer = UI::set_interval([this] { tick(); }, START_TICK_MS);
}

// Dipanggil setelah widget di-attach ke window dan ukuran di-set.
// Ini adalah tempat paling aman untuk request focus, karena _root_window
// sudah valid saat ini.
void TetrisWidget::on_layout_change(const Gfx::Rect& old_rect) {
	focus();
}

// Klik pada widget = ambil focus kembali (misal setelah alt-tab)
bool TetrisWidget::on_mouse_button(Pond::MouseButtonEvent evt) {
	focus();
	return true;
}

Gfx::Dimensions TetrisWidget::preferred_size() {
	return { TETRIS_W, TETRIS_H };
}

// ─── Game logic ───────────────────────────────────────────────────────────────

void TetrisWidget::game_pause() {
	m_state = State::START;
	m_dropping = false;
	memset(m_field, BLK_NONE, sizeof(m_field));
}

void TetrisWidget::game_start() {
	m_score = m_lines = m_pieces = 0;
	m_old_score = m_old_lines = m_old_pieces = 0;
	m_dropping = false;
	m_state = State::PLAYING;
	memset(m_field, BLK_NONE, sizeof(m_field));
	piece_new();
}

void TetrisWidget::piece_new() {
	memcpy(m_cur_piece, s_pieces[rand() % PIECE_COUNT], sizeof(Piece));
	m_cur_x = 5;
	m_cur_y = 0;
	m_pieces++;

	if (piece_collides(m_cur_piece, m_cur_x, m_cur_y)) {
		game_pause();
		m_state = State::GAMEOVER;
	}
}

void TetrisWidget::piece_rotate(Piece& p) {
	// In-place 90° clockwise rotation (4x4 matrix)
	for (int i = 0; i < 2; i++) {
		for (int j = i; j < 3 - i; j++) {
			Block tmp          = p[i][j];
			p[i][j]            = p[3-j][i];
			p[3-j][i]          = p[3-i][3-j];
			p[3-i][3-j]        = p[j][3-i];
			p[j][3-i]          = tmp;
		}
	}
}

bool TetrisWidget::piece_oob(const Piece& p, int px, int py) const {
	for (int by = 0; by < PIECE_H; by++) {
		for (int bx = 0; bx < PIECE_W; bx++) {
			if (p[by][bx] == BLK_NONE) continue;
			int fx = px + bx - PIECE_W/2;
			int fy = py + by - PIECE_H/2;
			if (fx < 0 || fx >= FIELD_WIDTH || fy >= FIELD_HEIGHT)
				return true;
		}
	}
	return false;
}

bool TetrisWidget::piece_collides(const Piece& p, int px, int py) const {
	for (int by = 0; by < PIECE_H; by++) {
		for (int bx = 0; bx < PIECE_W; bx++) {
			if (p[by][bx] == BLK_NONE) continue;
			int fx = px + bx - PIECE_W/2;
			int fy = py + by - PIECE_H/2;
			if (fx < 0 || fx >= FIELD_WIDTH || fy < 0 || fy >= FIELD_HEIGHT) continue;
			if (m_field[fy][fx] != BLK_NONE) return true;
		}
	}
	return false;
}

void TetrisWidget::piece_commit() {
	for (int by = 0; by < PIECE_H; by++) {
		for (int bx = 0; bx < PIECE_W; bx++) {
			if (m_cur_piece[by][bx] == BLK_NONE) continue;
			int fx = m_cur_x + bx - PIECE_W/2;
			int fy = m_cur_y + by - PIECE_H/2;
			if (fx >= 0 && fx < FIELD_WIDTH && fy >= 0 && fy < FIELD_HEIGHT)
				m_field[fy][fx] = m_cur_piece[by][bx];
		}
	}
}

void TetrisWidget::piece_drop_instant() {
	while (!piece_oob(m_cur_piece, m_cur_x, m_cur_y) &&
	       !piece_collides(m_cur_piece, m_cur_x, m_cur_y))
		m_cur_y++;
	m_cur_y--;
}

int TetrisWidget::ghost_y() const {
	int gy = m_cur_y;
	while (!piece_oob(m_cur_piece, m_cur_x, gy) &&
	       !piece_collides(m_cur_piece, m_cur_x, gy))
		gy++;
	return gy - 1;
}

void TetrisWidget::field_check_lines() {
	int found = 0;
	for (int y = 0; y < FIELD_HEIGHT; y++) {
		bool full = true;
		for (int x = 0; x < FIELD_WIDTH; x++) {
			if (m_field[y][x] == BLK_NONE) { full = false; break; }
		}
		if (full) {
			for (int x = 0; x < FIELD_WIDTH; x++)
				m_field[y][x] = BLK_CLEARING;
			m_lines++;
			found++;
		}
	}
	if (found > 0) {
		const int pts[] = {0, 40, 100, 300, 1200};
		m_score += pts[found < 5 ? found : 4];
		m_state = State::CLEARING;
		m_clear_ticks = 0;
		// Ganti timer ke CLEAR_TICK_MS untuk animasi blink
		m_dropping = false;
		m_timer->stop();
		m_timer = UI::set_interval([this] { tick(); }, CLEAR_TICK_MS);
	}
}

void TetrisWidget::field_move_down(int line) {
	for (int y = line; y >= 1; y--)
		memcpy(m_field[y], m_field[y-1], sizeof(Block) * FIELD_WIDTH);
	for (int x = 0; x < FIELD_WIDTH; x++)
		m_field[0][x] = BLK_NONE;
}

void TetrisWidget::field_clear_tick() {
	m_clear_ticks++;

	// Setelah 6 tick (~360ms pada CLEAR_TICK_MS=60ms), hapus semua baris CLEARING
	if (m_clear_ticks >= 6) {
		// Hapus dari bawah ke atas agar field_move_down benar
		for (int y = FIELD_HEIGHT - 1; y >= 0; y--) {
			bool is_clearing = false;
			for (int x = 0; x < FIELD_WIDTH; x++) {
				if (m_field[y][x] == BLK_CLEARING) { is_clearing = true; break; }
			}
			if (is_clearing) {
				field_move_down(y);
				y++; // cek baris yang sama lagi setelah digeser
			}
		}

		// Selesai — reset ke playing dengan timer normal
		m_dropping = false;
		m_timer->stop();
		m_timer = UI::set_interval([this] { tick(); }, TICK_MS);
		m_state = State::PLAYING;
		piece_new();
	}
}

// ─── Tick (dipanggil timer) ───────────────────────────────────────────────────

void TetrisWidget::tick() {
	switch (m_state) {
	case State::START:
	case State::GAMEOVER:
		m_blink = !m_blink;
		repaint();
		// timer sudah diset START_TICK_MS oleh initialize(), tidak perlu reschedule
		return;

	case State::CLEARING:
		field_clear_tick();
		repaint();
		return;

	case State::PLAYING: {
		// Turunkan piece satu baris
		if (piece_oob(m_cur_piece, m_cur_x, m_cur_y + 1) ||
		    piece_collides(m_cur_piece, m_cur_x, m_cur_y + 1))
		{
			piece_commit();
			field_check_lines();
			if (m_state == State::PLAYING)
				piece_new();
		} else {
			m_cur_y++;
		}
		repaint();
		return;
	}
	}
}

// ─── Keyboard ─────────────────────────────────────────────────────────────────
//
// Pond::KeyEvent fields (lihat Event.h):
//   evt.scancode  — raw PS/2 scancode (bit 0x80 set = key release)
//   evt.key       — scancode yang sudah di-decode (tanpa bit release)
//   evt.character — karakter ASCII (0 untuk non-printable / special key)
//
// KBD_ISPRESSED(evt): true jika key ditekan (bit 0x80 di scancode == 0)
//
// Scancode standar PS/2 set 1 untuk arrow keys:
//   Up=0x48, Down=0x50, Left=0x4B, Right=0x4D
// Enter = scancode 0x1C, character '\r' (0x0D)

#ifndef KEY_ENTER
#  define KEY_ENTER   0x1C
#endif
#ifndef KEY_LEFT
#  define KEY_LEFT    0x4B
#endif
#ifndef KEY_RIGHT
#  define KEY_RIGHT   0x4D
#endif
#ifndef KEY_UP
#  define KEY_UP      0x48
#endif
#ifndef KEY_DOWN
#  define KEY_DOWN    0x50
#endif

bool TetrisWidget::on_keyboard(Pond::KeyEvent evt) {
	// pressed = true saat tombol ditekan, false saat dilepas
	bool pressed = KBD_ISPRESSED(evt);

	if (m_state == State::START || m_state == State::GAMEOVER) {
		// Enter: scancode 0x1C atau character '\r'/'\n'
		if (pressed && (evt.key == KEY_ENTER ||
		                evt.character == '\r' || evt.character == '\n'))
		{
			game_start();
			m_timer->stop();
			m_timer = UI::set_interval([this] { tick(); }, TICK_MS);
			repaint();
		}
		return true;
	}

	if (m_state == State::CLEARING) return true;

	// A / Arrow Left — gerak kiri
	if (pressed && (evt.character == 'a' || evt.key == KEY_LEFT)) {
		int nx = m_cur_x - 1;
		if (!piece_oob(m_cur_piece, nx, m_cur_y) &&
		    !piece_collides(m_cur_piece, nx, m_cur_y))
			m_cur_x = nx;
		repaint();

	// D / Arrow Right — gerak kanan
	} else if (pressed && (evt.character == 'd' || evt.key == KEY_RIGHT)) {
		int nx = m_cur_x + 1;
		if (!piece_oob(m_cur_piece, nx, m_cur_y) &&
		    !piece_collides(m_cur_piece, nx, m_cur_y))
			m_cur_x = nx;
		repaint();

	// R / Arrow Up — rotasi
	} else if (pressed && (evt.character == 'r' || evt.key == KEY_UP)) {
		Piece rotated;
		memcpy(rotated, m_cur_piece, sizeof(Piece));
		piece_rotate(rotated);
		if (!piece_oob(rotated, m_cur_x, m_cur_y) &&
		    !piece_collides(rotated, m_cur_x, m_cur_y))
			memcpy(m_cur_piece, rotated, sizeof(Piece));
		repaint();

	// S / Arrow Down — soft drop (percepat saat tahan)
	} else if (pressed && (evt.character == 's' || evt.key == KEY_DOWN)) {
		if (!m_dropping) {
			m_dropping = true;
			m_timer->stop();
			m_timer = UI::set_interval([this] { tick(); }, DROP_TICK_MS);
		}

	// Lepas S / Arrow Down — kembalikan kecepatan normal
	} else if (!pressed && (evt.character == 's' || evt.key == KEY_DOWN)) {
		if (m_dropping) {
			m_dropping = false;
			m_timer->stop();
			m_timer = UI::set_interval([this] { tick(); }, TICK_MS);
		}

	// Spasi — hard drop langsung
	} else if (pressed && evt.character == ' ') {
		piece_drop_instant();
		piece_commit();
		field_check_lines();
		if (m_state == State::PLAYING)
			piece_new();
		repaint();
	}

	return true;
}

// ─── Draw helpers ─────────────────────────────────────────────────────────────

// Gambar satu blok di koordinat field (fx, fy)
// Menggunakan ctx.fill() karena DrawContext tidak punya draw_line.
// Efek 3D: highlight atas/kiri, shadow bawah/kanan, fill tengah.
void TetrisWidget::draw_block(const UI::DrawContext& ctx, Block b, int fx, int fy) const {
	int px = FIELD_LEFT + fx * BLOCK_SIZE;
	int py = FIELD_TOP  + fy * BLOCK_SIZE;
	int sz = BLOCK_SIZE;

	if (b == BLK_NONE) {
		ctx.fill({ px, py, sz, sz }, Gfx::Color(0x1a, 0x1a, 0x2e));
		return;
	}

	Gfx::Color nc = s_normal[b];
	Gfx::Color hc = s_highlight[b];
	Gfx::Color sc = s_shadow[b];

	if (b == BLK_OUTLINE) {
		// Ghost piece: hanya outline tipis
		ctx.fill({ px, py, sz, 1 }, sc);
		ctx.fill({ px, py, 1, sz }, sc);
		ctx.fill({ px, py+sz-1, sz, 1 }, sc);
		ctx.fill({ px+sz-1, py, 1, sz }, sc);
		return;
	}

	// Border luar
	ctx.fill({ px,       py,       sz, 1  }, hc); // atas
	ctx.fill({ px,       py,       1,  sz }, hc); // kiri
	ctx.fill({ px,       py+sz-1,  sz, 1  }, sc); // bawah
	ctx.fill({ px+sz-1,  py,       1,  sz }, sc); // kanan

	// Fill tengah
	ctx.fill({ px+1, py+1, sz-2, sz-2 }, nc);

	// Inner bevel (kedalaman 3px)
	if (sz > 6) {
		ctx.fill({ px+3, py+3, sz-6, 1   }, sc);
		ctx.fill({ px+3, py+3, 1,   sz-6 }, sc);
		ctx.fill({ px+3, py+sz-4, sz-6, 1   }, hc);
		ctx.fill({ px+sz-4, py+3, 1,   sz-6 }, hc);
	}
}

void TetrisWidget::draw_field(const UI::DrawContext& ctx) {
	bool blink_on = (m_state == State::CLEARING) && (m_clear_ticks % 2 == 0);
	for (int y = 0; y < FIELD_HEIGHT; y++) {
		for (int x = 0; x < FIELD_WIDTH; x++) {
			Block b = m_field[y][x];
			// Efek blink: saat genap tampilkan CLEARING sebagai NONE (gelap)
			if (b == BLK_CLEARING && blink_on)
				b = BLK_NONE;
			draw_block(ctx, b, x, y);
		}
	}
}

void TetrisWidget::draw_field_border(const UI::DrawContext& ctx) const {
	// Background field
	ctx.fill({ FIELD_LEFT, FIELD_TOP,
	           FIELD_WIDTH * BLOCK_SIZE,
	           FIELD_HEIGHT * BLOCK_SIZE }, Gfx::Color(0x1a, 0x1a, 0x2e));
	// Border luar
	ctx.fill({ FIELD_LEFT - 2, FIELD_TOP - 2,
	           FIELD_WIDTH * BLOCK_SIZE + 4, 2 }, Gfx::Color(0x88, 0x88, 0xaa));
	ctx.fill({ FIELD_LEFT - 2, FIELD_BOTTOM,
	           FIELD_WIDTH * BLOCK_SIZE + 4, 2 }, Gfx::Color(0x44, 0x44, 0x66));
	ctx.fill({ FIELD_LEFT - 2, FIELD_TOP - 2,
	           2, FIELD_HEIGHT * BLOCK_SIZE + 4 }, Gfx::Color(0x88, 0x88, 0xaa));
	ctx.fill({ FIELD_RIGHT, FIELD_TOP - 2,
	           2, FIELD_HEIGHT * BLOCK_SIZE + 4 }, Gfx::Color(0x44, 0x44, 0x66));
}

// draw_field sudah menggambar seluruh field setiap frame, sehingga
// tidak perlu hapus posisi lama — cukup gambar ghost dan piece di atas.
void TetrisWidget::draw_current_piece(const UI::DrawContext& ctx) const {
	if (m_state != State::PLAYING && m_state != State::CLEARING) return;

	int gy = ghost_y();

	// Ghost (outline) — hanya di sel yang kosong di field
	for (int by = 0; by < PIECE_H; by++) {
		for (int bx = 0; bx < PIECE_W; bx++) {
			if (m_cur_piece[by][bx] == BLK_NONE) continue;
			int fx = m_cur_x + bx - PIECE_W/2;
			int fy = gy      + by - PIECE_H/2;
			if (fx >= 0 && fx < FIELD_WIDTH && fy >= 0 && fy < FIELD_HEIGHT)
				if (m_field[fy][fx] == BLK_NONE)
					draw_block(ctx, BLK_OUTLINE, fx, fy);
		}
	}

	// Piece aktif
	for (int by = 0; by < PIECE_H; by++) {
		for (int bx = 0; bx < PIECE_W; bx++) {
			if (m_cur_piece[by][bx] == BLK_NONE) continue;
			int fx = m_cur_x + bx - PIECE_W/2;
			int fy = m_cur_y + by - PIECE_H/2;
			if (fx >= 0 && fx < FIELD_WIDTH && fy >= 0 && fy < FIELD_HEIGHT)
				draw_block(ctx, m_cur_piece[by][bx], fx, fy);
		}
	}
}

// Helper: gambar satu baris statistik di side panel
void TetrisWidget::draw_stat(const UI::DrawContext& ctx, int y_offset,
                              const char* label, uint64_t val) const
{
	char buf[16];
	snprintf(buf, sizeof(buf), "%06llu", (unsigned long long)val);

	ctx.draw_text(label, { SIDE_LEFT + 6, y_offset, SIDE_W - 12, 14 },
	              UI::TextAlignment::BEGINNING, UI::TextAlignment::CENTER,
	              UI::Theme::font(), Gfx::Color(0xcc, 0xcc, 0xee));
	ctx.draw_text(buf, { SIDE_LEFT + 6, y_offset + 16, SIDE_W - 12, 14 },
	              UI::TextAlignment::END, UI::TextAlignment::CENTER,
	              UI::Theme::font(), Gfx::Color(0xff, 0xff, 0x80));
}

void TetrisWidget::draw_side_panel(const UI::DrawContext& ctx) const {
	// Background panel
	ctx.fill({ SIDE_LEFT, SIDE_TOP, SIDE_W, SIDE_BOTTOM - SIDE_TOP },
	          Gfx::Color(0x14, 0x14, 0x24));
	// Border
	ctx.fill({ SIDE_LEFT,   SIDE_TOP, 1, SIDE_BOTTOM - SIDE_TOP }, Gfx::Color(0x44,0x44,0x66));
	ctx.fill({ SIDE_RIGHT-1,SIDE_TOP, 1, SIDE_BOTTOM - SIDE_TOP }, Gfx::Color(0x44,0x44,0x66));

	int y = SIDE_TOP + 10;
	draw_stat(ctx, y,        "SCORE",  m_score);
	draw_stat(ctx, y + 50,   "LINES",  m_lines);
	draw_stat(ctx, y + 100,  "PIECES", m_pieces);

	// Controls help
	int hy = SIDE_BOTTOM - 110;
	auto help = [&](const char* txt, int dy) {
		ctx.draw_text(txt, { SIDE_LEFT + 6, hy + dy, SIDE_W - 12, 12 },
		              UI::TextAlignment::BEGINNING, UI::TextAlignment::CENTER,
		              UI::Theme::font(), Gfx::Color(0x77, 0x77, 0x99));
	};
	help("A/D - Move",    0);
	help("S   - Soft drop", 14);
	help("SPC - Hard drop", 28);
	help("R/Up- Rotate",  42);
	help("Enter - Start", 56);
}

void TetrisWidget::draw_start_screen(const UI::DrawContext& ctx) const {
	// "TETRIS" warna-warni di tengah field
	const Gfx::Color tc[] = {
		s_normal[BLK_RED], s_normal[BLK_ORANGE], s_normal[BLK_YELLOW],
		s_normal[BLK_GREEN], s_normal[BLK_CYAN], s_normal[BLK_BLUE]
	};
	const char letters[] = "TETRIS";
	int total_w = 6 * 22;
	int sx = FIELD_LEFT + (FIELD_WIDTH * BLOCK_SIZE - total_w) / 2;
	int sy = FIELD_TOP + (FIELD_HEIGHT * BLOCK_SIZE) / 3;
	for (int i = 0; i < 6; i++) {
		char ch[2] = { letters[i], '\0' };
		ctx.draw_text(ch, { sx + i * 22, sy, 22, 28 },
		              UI::TextAlignment::CENTER, UI::TextAlignment::CENTER,
		              UI::Theme::font(), tc[i]);
	}

	// Blink "PRESS ENTER"
	if (m_blink) {
		ctx.draw_text("PRESS ENTER", { FIELD_LEFT, sy + 50, FIELD_WIDTH * BLOCK_SIZE, 14 },
		              UI::TextAlignment::CENTER, UI::TextAlignment::CENTER,
		              UI::Theme::font(), Gfx::Color(0xee, 0xee, 0xee));
	}
}

void TetrisWidget::draw_gameover(const UI::DrawContext& ctx) const {
	int fw = FIELD_WIDTH * BLOCK_SIZE;
	int cx = FIELD_LEFT;
	int cy = FIELD_TOP + FIELD_HEIGHT * BLOCK_SIZE / 2 - 20;

	ctx.fill({ cx, cy - 6, fw, 44 }, Gfx::Color(0x00, 0x00, 0x00, 0xcc));
	ctx.draw_text("GAME OVER", { cx, cy, fw, 16 },
	              UI::TextAlignment::CENTER, UI::TextAlignment::CENTER,
	              UI::Theme::font(), Gfx::Color(0xFF, 0x44, 0x44));
	if (m_blink) {
		ctx.draw_text("PRESS ENTER", { cx, cy + 20, fw, 14 },
		              UI::TextAlignment::CENTER, UI::TextAlignment::CENTER,
		              UI::Theme::font(), Gfx::Color(0xee, 0xee, 0xee));
	}
}

// ─── do_repaint ───────────────────────────────────────────────────────────────

void TetrisWidget::do_repaint(const UI::DrawContext& ctx) {
	// Background window
	ctx.fill({ 0, 0, TETRIS_W, TETRIS_H }, Gfx::Color(0x10, 0x10, 0x20));

	draw_field_border(ctx);
	draw_field(ctx);
	draw_current_piece(ctx);
	draw_side_panel(ctx);

	if (m_state == State::START)
		draw_start_screen(ctx);
	else if (m_state == State::GAMEOVER)
		draw_gameover(ctx);
}