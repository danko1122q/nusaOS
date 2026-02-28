/*
	This file is part of nusaOS.
	Copyright (c) nusaOS 2026. All rights reserved.
*/

#pragma once

#define GAME_WIDTH         540
#define GAME_HEIGHT        400
#define COLUMN_PAIRS       3
#define COLUMN_WIDTH       60
#define COLUMN_SEPARATION  200
#define BIRD_W             54
#define BIRD_H             32

// Physics per-second agar delta-time mulus lepas dari framerate/timer jitter.
// Kalau timer slip 1 frame, physics tetap advance sebesar waktu nyata yang berlalu,
// bukan loncat 2 tick sekaligus — ini yang membuat gerakan macet di versi lama.
#define GRAVITY_PER_SEC      600.0f
#define MAX_VELOCITY_PER_SEC 450.0f
#define PIPE_SPEED_PER_SEC   120.0f
#define JUMP_VELOCITY       -280.0f   // px/s ke atas (negatif = ke atas)

enum class GameState {
	WAITING,
	PLAYING,
	DEAD
};

struct Pipe {
	float x;    // float agar sub-pixel movement mulus
	int   top_h;
};

struct FloppyGame {
	GameState state     = GameState::WAITING;
	float bird_y        = GAME_HEIGHT / 2.0f - BIRD_H / 2.0f;
	float bird_velocity = 0.0f;   // px/s, positif = ke bawah
	int   score         = 0;
	int   seed          = 0;
	Pipe  pipes[COLUMN_PAIRS];

	void init();
	void tick(float dt);   // dt dalam detik
	void jump();
	bool bird_dead() const;

private:
	bool collides(const Pipe& p) const;
	static int next_seed(int s) { return (739 * s + 24) % 97; }
};