/*
	This file is part of nusaOS.

	nusaOS is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	nusaOS is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with nusaOS.  If not, see <https://www.gnu.org/licenses/>.

	Copyright (c) danko1122q 2026. All rights reserved.
*/

#pragma once

#include <libui/widget/Widget.h>
#include <libui/Timer.h>
#include <libgraphics/Graphics.h>
#include <libnusa/Time.h>
#include <ctime>
#include <cmath>

// Ukuran widget jam
#define CLOCK_WIDTH  140
#define CLOCK_HEIGHT 178

// Radius jam analog
#define CLOCK_CENTER_X 70
#define CLOCK_CENTER_Y 70
#define CLOCK_RADIUS   60

// Warna-warna
#define COLOR_CLOCK_BG        Gfx::Color(30,  30,  40)
#define COLOR_CLOCK_FACE      Gfx::Color(240, 240, 245)
#define COLOR_CLOCK_FACE_RIM  Gfx::Color(180, 180, 195)
#define COLOR_CLOCK_CENTER    Gfx::Color(60,  60,  70)
#define COLOR_HOUR_HAND       Gfx::Color(40,  40,  50)
#define COLOR_MINUTE_HAND     Gfx::Color(40,  40,  50)
#define COLOR_SECOND_HAND     Gfx::Color(210, 50,  50)
#define COLOR_TICK_MAJOR      Gfx::Color(80,  80,  90)
#define COLOR_TICK_MINOR      Gfx::Color(160, 160, 175)
#define COLOR_TEXT_TIME       Gfx::Color(220, 220, 230)
#define COLOR_TEXT_DATE       Gfx::Color(160, 160, 175)

// Update tiap 1000ms
#define CLOCK_TICK_MS 1000

class ClockWidget : public UI::Widget {
public:
	WIDGET_DEF(ClockWidget)

	// Struct ini public agar static free function di .cpp bisa menggunakannya
	struct ClockTime {
		int hour;    // 0-23
		int minute;  // 0-59
		int second;  // 0-59
		int day;
		int month;   // 0-11 (tm_mon)
		int year;    // tahun penuh
	};

	Gfx::Dimensions preferred_size() override;
	void do_repaint(const UI::DrawContext& ctx) override;

protected:
	void initialize() override;

private:
	ClockWidget();

	static Gfx::Point hand_endpoint(int cx, int cy, float angle, float length);

	void draw_line(const UI::DrawContext& ctx,
	               Gfx::Point from, Gfx::Point to,
	               Gfx::Color color, bool thick = false) const;

	void draw_hand(const UI::DrawContext& ctx,
	               Gfx::Point from, Gfx::Point to,
	               Gfx::Color color, bool thick) const;

	void draw_ticks(const UI::DrawContext& ctx) const;
	void draw_hour_labels(const UI::DrawContext& ctx) const;

	static ClockTime get_time();

	Duck::Ptr<UI::Timer> m_timer;
	static const char* s_months[12];
};