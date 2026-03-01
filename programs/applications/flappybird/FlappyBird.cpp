/*
	This file is part of nusaOS.
	Copyright (c) nusaOS 2026. All rights reserved.
*/

#include "FlappyBird.h"

void FloppyGame::init() {
	bird_y        = GAME_HEIGHT / 2.0f - BIRD_H / 2.0f;
	bird_velocity = 0.0f;
	score         = 0;
	seed          = 0;
	state         = GameState::WAITING;

	for (int i = 0; i < COLUMN_PAIRS; i++) {
		int top_h = ((1000 * i) % 31) * 4 + 50;
		pipes[i] = {
			(float)(GAME_WIDTH * 0.9f) + i * 200.0f,
			top_h,
			false
		};
	}
}

void FloppyGame::jump() {
	// Set velocity langsung ke JUMP_VELOCITY (ke atas).
	// Jika sudah bergerak ke atas, berikan sedikit boost tambahan
	// agar double-tap terasa responsif, tapi dibatasi agar tidak overpowered.
	if (bird_velocity < 0.0f)
		bird_velocity = bird_velocity * 0.3f + JUMP_VELOCITY * 0.7f;
	else
		bird_velocity = JUMP_VELOCITY;
}

bool FloppyGame::collides(const Pipe& p) const {
	const float bx1 = GAME_WIDTH / 2.0f - BIRD_W / 2.0f;
	const float bx2 = bx1 + BIRD_W;
	const float by1 = bird_y;
	const float by2 = bird_y + BIRD_H;

	if (bx2 <= p.x || bx1 >= p.x + COLUMN_WIDTH)
		return false;

	if (by1 < p.top_h)
		return true;

	float bot_y = p.top_h + COLUMN_SEPARATION;
	if (by2 > bot_y)
		return true;

	return false;
}

void FloppyGame::tick(float dt) {
	if (state != GameState::PLAYING)
		return;

	// Clamp dt agar tidak ada lompatan besar saat window di-focus ulang
	// atau saat sistem lag parah (misal > 200ms)
	if (dt > 0.1f) dt = 0.1f;
	if (dt <= 0.0f) return;

	// ── Gerakkan pipe ────────────────────────────────────────────────────────
	for (int i = 0; i < COLUMN_PAIRS; i++) {
		pipes[i].x -= PIPE_SPEED_PER_SEC * dt;

		if (pipes[i].x + COLUMN_WIDTH <= 0.0f) {
			pipes[i].x = (float) GAME_WIDTH;

			seed = next_seed(seed);
			int yend = seed * 2 + 20;
			if (yend > GAME_HEIGHT - COLUMN_SEPARATION)
				yend = GAME_HEIGHT - COLUMN_SEPARATION - 20;
			pipes[i].top_h = yend;
			pipes[i].scored = false;
		}

		// Score saat burung berhasil melewati tengah pipe (bukan saat pipe keluar layar)
		const float bird_cx = GAME_WIDTH / 2.0f;
		if (!pipes[i].scored && pipes[i].x + COLUMN_WIDTH < bird_cx) {
			pipes[i].scored = true;
			score++;
		}

		if (collides(pipes[i])) {
			state = GameState::DEAD;
			return;
		}
	}

	// ── Fisika burung ─────────────────────────────────────────────────────────
	bird_velocity += GRAVITY_PER_SEC * dt;
	if (bird_velocity >  MAX_VELOCITY_PER_SEC) bird_velocity =  MAX_VELOCITY_PER_SEC;
	if (bird_velocity < -MAX_VELOCITY_PER_SEC) bird_velocity = -MAX_VELOCITY_PER_SEC;
	bird_y += bird_velocity * dt;

	// ── Batas layar ───────────────────────────────────────────────────────────
	if (bird_y + BIRD_H < 0.0f || bird_y > GAME_HEIGHT)
		state = GameState::DEAD;
}

bool FloppyGame::bird_dead() const {
	return state == GameState::DEAD;
}