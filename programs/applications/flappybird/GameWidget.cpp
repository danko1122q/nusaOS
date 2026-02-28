/*
	This file is part of nusaOS.
	Copyright (c) nusaOS 2026. All rights reserved.
*/

#include "GameWidget.h"
#include <libui/libui.h>
#include <libkeyboard/Keyboard.h>

// ─── Constructor ─────────────────────────────────────────────────────────────

GameWidget::GameWidget() {
	set_uses_alpha(false);
	m_game.init();
	m_last_tick = Duck::Time::now();

	// Timer TIDAK pernah di-stop — selalu jalan agar repaint terus terjadi.
	// Physics di-pause lewat state (WAITING/DEAD), bukan lewat timer stop/start.
	// Ini mencegah m_last_tick stale yang menyebabkan dt raksasa saat game resume.
	m_timer = UI::set_interval([this] {
		auto now = Duck::Time::now();
		float dt = (float)(now - m_last_tick).millis() / 1000.0f;
		m_last_tick = now;

		if (m_game.state == GameState::PLAYING) {
			m_game.tick(dt);
		}

		repaint();
	}, TICK_MS);
	// Timer langsung jalan dari awal — tidak di-stop
}

void GameWidget::initialize() {
	focus();
}

// ─── Public ──────────────────────────────────────────────────────────────────

void GameWidget::reset() {
	m_game.init();
	// m_last_tick tidak perlu di-reset — timer sudah jalan terus,
	// dt akan normal di tick berikutnya
	repaint();
}

// ─── Input ───────────────────────────────────────────────────────────────────

void GameWidget::do_jump() {
	if (m_game.state == GameState::WAITING) {
		m_game.init();
		m_game.state = GameState::PLAYING;
		// Reset m_last_tick agar dt pertama setelah start = ~0,
		// bukan akumulasi sejak app dibuka
		m_last_tick = Duck::Time::now();
		return;
	}

	if (m_game.state == GameState::DEAD) {
		m_game.init();
		m_game.state = GameState::PLAYING;
		m_last_tick = Duck::Time::now();
		return;
	}

	m_game.jump();
}

bool GameWidget::on_mouse_button(Pond::MouseButtonEvent evt) {
	if ((evt.new_buttons & POND_MOUSE1) && !(evt.old_buttons & POND_MOUSE1)) {
		do_jump();
		return true;
	}
	return false;
}

bool GameWidget::on_keyboard(Pond::KeyEvent evt) {
	if (!KBD_ISPRESSED(evt))
		return false;
	if ((Keyboard::Key) evt.key == Keyboard::Space) {
		do_jump();
		return true;
	}
	return false;
}

// ─── Drawing ─────────────────────────────────────────────────────────────────

void GameWidget::draw_pipe(const UI::DrawContext& ctx, const Pipe& p) const {
	const int cap_overhang = 6;
	const int cap_h        = 12;
	const int px           = (int) p.x;

	if (p.top_h > 0) {
		ctx.fill({ px, 0, COLUMN_WIDTH, p.top_h }, COLOR_PIPE);
		ctx.fill({ px - cap_overhang, p.top_h - cap_h,
		           COLUMN_WIDTH + cap_overhang * 2, cap_h }, COLOR_PIPE);
		ctx.fill({ px, 0, 4, p.top_h }, COLOR_PIPE_CAP);
	}

	int bot_y = p.top_h + COLUMN_SEPARATION;
	int bot_h = GAME_HEIGHT - bot_y;
	if (bot_h > 0) {
		ctx.fill({ px - cap_overhang, bot_y,
		           COLUMN_WIDTH + cap_overhang * 2, cap_h }, COLOR_PIPE);
		ctx.fill({ px, bot_y + cap_h, COLUMN_WIDTH, bot_h - cap_h }, COLOR_PIPE);
		ctx.fill({ px, bot_y, 4, bot_h }, COLOR_PIPE_CAP);
	}
}

void GameWidget::draw_bird(const UI::DrawContext& ctx) const {
	const int bx = GAME_WIDTH / 2 - BIRD_W / 2;
	const int by = (int) m_game.bird_y;

	ctx.fill({ bx + 4, by + 6,  BIRD_W - 10, BIRD_H - 10 }, COLOR_BIRD);
	ctx.fill({ bx + 6, by + BIRD_H / 2, BIRD_W / 3, BIRD_H / 3 },
	         Gfx::Color(230, 100, 80));
	ctx.fill({ bx, by + BIRD_H / 2 - 4, 8, 8 }, COLOR_BIRD);
	ctx.fill({ bx + BIRD_W - 18, by + 2, 18, 20 }, Gfx::Color(225, 85, 65));
	ctx.fill({ bx + BIRD_W - 10, by + 6,  8, 8 }, Gfx::Color(255, 255, 255));
	ctx.fill({ bx + BIRD_W -  8, by + 8,  4, 4 }, Gfx::Color(0, 0, 0));
	ctx.fill({ bx + BIRD_W - 2, by + 12, 10, 5 }, COLOR_BUTTON);
}

void GameWidget::draw_overlay(const UI::DrawContext& ctx) const {
	auto score_str = std::to_string(m_game.score);
	ctx.draw_text(score_str.c_str(),
	              { GAME_WIDTH / 2 - 30, 10, 60, 24 },
	              UI::TextAlignment::CENTER, UI::TextAlignment::CENTER,
	              UI::Theme::font(), COLOR_SCORE);

	if (m_game.state == GameState::WAITING) {
		const int pw = 220, ph = 60;
		const int px = GAME_WIDTH  / 2 - pw / 2;
		const int py = GAME_HEIGHT / 2 - ph / 2;
		ctx.fill({ px, py, pw, ph }, COLOR_BUTTON);
		ctx.draw_text("Click or SPACE to start",
		              { px + 5, py + 10, pw - 10, 20 },
		              UI::TextAlignment::CENTER, UI::TextAlignment::CENTER,
		              UI::Theme::font(), COLOR_TEXT);
		ctx.draw_text("(click = flap mid-game)",
		              { px + 5, py + 32, pw - 10, 18 },
		              UI::TextAlignment::CENTER, UI::TextAlignment::CENTER,
		              UI::Theme::font(), COLOR_TEXT);

	} else if (m_game.state == GameState::DEAD) {
		const int pw = 220, ph = 80;
		const int px = GAME_WIDTH  / 2 - pw / 2;
		const int py = GAME_HEIGHT / 2 - ph / 2;
		ctx.fill({ px, py, pw, ph }, COLOR_BUTTON);
		ctx.draw_text("Game Over",
		              { px + 5, py + 8, pw - 10, 22 },
		              UI::TextAlignment::CENTER, UI::TextAlignment::CENTER,
		              UI::Theme::font(), COLOR_GAMEOVER);
		auto result = "Score: " + std::to_string(m_game.score);
		ctx.draw_text(result.c_str(),
		              { px + 5, py + 32, pw - 10, 20 },
		              UI::TextAlignment::CENTER, UI::TextAlignment::CENTER,
		              UI::Theme::font(), COLOR_TEXT);
		ctx.draw_text("Click or SPACE to retry",
		              { px + 5, py + 54, pw - 10, 18 },
		              UI::TextAlignment::CENTER, UI::TextAlignment::CENTER,
		              UI::Theme::font(), COLOR_TEXT);
	}
}

Gfx::Dimensions GameWidget::preferred_size() {
	return { GAME_WIDTH, GAME_HEIGHT };
}

void GameWidget::do_repaint(const UI::DrawContext& ctx) {
	ctx.fill({ 0, 0, GAME_WIDTH, GAME_HEIGHT }, COLOR_SKY);

	for (int i = 0; i < COLUMN_PAIRS; i++)
		draw_pipe(ctx, m_game.pipes[i]);

	ctx.fill({ 0, GAME_HEIGHT - 18, GAME_WIDTH, 18 }, COLOR_GROUND);
	ctx.fill({ 0, GAME_HEIGHT - 18, GAME_WIDTH,  3 }, COLOR_GROUND_LINE);

	draw_bird(ctx);
	draw_overlay(ctx);
}