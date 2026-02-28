/*
	This file is part of nusaOS.
	Copyright (c) nusaOS 2026. All rights reserved.
*/

#pragma once

#include "FlappyBird.h"
#include <libui/widget/Widget.h>
#include <libui/Timer.h>
#include <libgraphics/Graphics.h>
#include <libnusa/Time.h>

// Colors (match original floppybird.c palette)
#define COLOR_SKY         Gfx::Color(113, 197, 207)
#define COLOR_BIRD        Gfx::Color(212,  68,  55)
#define COLOR_PIPE        Gfx::Color(113, 191,  46)
#define COLOR_PIPE_CAP    Gfx::Color( 80, 140,  28)
#define COLOR_BUTTON      Gfx::Color(244, 180,   0)
#define COLOR_TEXT        Gfx::Color(  0,   0,   0)
#define COLOR_SCORE       Gfx::Color(255, 255, 255)
#define COLOR_GAMEOVER    Gfx::Color(200,  40,  40)
#define COLOR_GROUND      Gfx::Color(150, 200,  80)
#define COLOR_GROUND_LINE Gfx::Color( 80, 140,  30)

// Poll timer setiap 16ms (~60fps polling).
// Physics menggunakan delta-time nyata jadi framerate display tidak kritis —
// timer ini hanya memastikan repaint sering dipanggil.
#define TICK_MS 16

class GameWidget : public UI::Widget {
public:
	WIDGET_DEF(GameWidget)

	void reset();

	Gfx::Dimensions preferred_size() override;
	void do_repaint(const UI::DrawContext& ctx) override;
	bool on_mouse_button(Pond::MouseButtonEvent evt) override;
	bool on_keyboard(Pond::KeyEvent evt) override;

protected:
	void initialize() override;

private:
	GameWidget();

	void do_jump();
	void draw_pipe(const UI::DrawContext& ctx, const Pipe& p) const;
	void draw_bird(const UI::DrawContext& ctx) const;
	void draw_overlay(const UI::DrawContext& ctx) const;

	FloppyGame    m_game;
	Duck::Ptr<UI::Timer> m_timer;
	Duck::Time    m_last_tick;    // waktu tick terakhir untuk hitung delta-time
};