/*
	This file is part of nusaOS.
	SPDX-License-Identifier: GPL-3.0-or-later
	Copyright (c) danko1122q 2026. All rights reserved.
*/

#include "ClockWidget.h"
#include <libui/libui.h>
#include <cmath>
#include <ctime>
#include <cstdlib>

const char* ClockWidget::s_months[12] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static constexpr float PI = 3.14159265358979323846f;

// Kalender helper untuk bypass localtime() yang mungkin buggy di nusaOS
#define LEAPYEAR(y) (((y) % 4 == 0) && (((y) % 100 != 0) || ((y) % 400 == 0)))

static const int s_days_per_month[12] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static ClockWidget::ClockTime timestamp_to_time(time_t ts) {
	// Jaga-jaga jika timestamp negatif (RTC belum diset atau CMOS invalid)
	if (ts < 0) ts = 0;

	// Pisahkan satuan waktu dasar
	int second = ts % 60; ts /= 60;
	int minute = ts % 60; ts /= 60;
	int hour   = ts % 24; ts /= 24;
	// ts sekarang berisi jumlah hari sejak 1 Jan 1970 (Epoch)

	// Rekonstruksi tahun
	int year = 1970;
	while (true) {
		int days_in_year = LEAPYEAR(year) ? 366 : 365;
		if (ts < days_in_year)
			break;
		ts -= days_in_year;
		year++;
		// Batasi tahun agar tidak loop selamanya jika data korup
		if (year > 2100) { year = 2026; ts = 0; break; }
	}

	// Rekonstruksi bulan (0-indexed seperti struktur tm)
	int month = 0; 
	for (int m = 0; m < 12; m++) {
		int dim = s_days_per_month[m];
		if (m == 1 && LEAPYEAR(year)) dim = 29; // Cek kabisat Februari
		if (ts < dim) { month = m; break; }
		ts -= dim;
	}

	int day = (int)ts + 1; // Jadikan 1-indexed

	return { hour, minute, second, day, month, year };
}

// Constructor
ClockWidget::ClockWidget() {
	set_uses_alpha(false);
}

void ClockWidget::initialize() {
	m_timer = UI::set_interval([this] {
		repaint();
	}, CLOCK_TICK_MS);
}

// Ukuran widget
Gfx::Dimensions ClockWidget::preferred_size() {
	return { CLOCK_WIDTH, CLOCK_HEIGHT };
}

// Ambil waktu sistem
ClockWidget::ClockTime ClockWidget::get_time() {
	time_t raw = time(nullptr);
	// Gunakan parser manual karena localtime() nusaOS libc terkadang
	// kena bug off-by-one pada nilai tm_mday.
	return timestamp_to_time(raw);
}

// Kalkulasi geometri ujung jarum jam
Gfx::Point ClockWidget::hand_endpoint(int cx, int cy, float angle_rad, float length) {
	float a = angle_rad - PI / 2.0f;
	return { (int)(cx + cosf(a) * length), (int)(cy + sinf(a) * length) };
}

// Implementasi garis Bresenham
void ClockWidget::draw_line(const UI::DrawContext& ctx,
                             Gfx::Point from, Gfx::Point to,
                             Gfx::Color color, bool thick) const
{
	int x0 = from.x, y0 = from.y;
	int x1 = to.x,   y1 = to.y;
	int dx = abs(x1 - x0);
	int dy = abs(y1 - y0);
	int sx = (x0 < x1) ? 1 : -1;
	int sy = (y0 < y1) ? 1 : -1;
	int err = dx - dy;

	while (true) {
		ctx.fill({ x0, y0, 1, 1 }, color);
		if (thick) {
			if (dx >= dy)
				ctx.fill({ x0, y0 + 1, 1, 1 }, color);
			else
				ctx.fill({ x0 + 1, y0, 1, 1 }, color);
		}
		if (x0 == x1 && y0 == y1) break;
		int e2 = 2 * err;
		if (e2 > -dy) { err -= dy; x0 += sx; }
		if (e2 <  dx) { err += dx; y0 += sy; }
	}
}

void ClockWidget::draw_hand(const UI::DrawContext& ctx,
                             Gfx::Point from, Gfx::Point to,
                             Gfx::Color color, bool thick) const
{
	draw_line(ctx, from, to, color, thick);
}

// Gambar garis-garis detik/menit (ticks)
void ClockWidget::draw_ticks(const UI::DrawContext& ctx) const {
	const int cx = CLOCK_CENTER_X;
	const int cy = CLOCK_CENTER_Y;
	const int r  = CLOCK_RADIUS;

	for (int i = 0; i < 60; i++) {
		float angle = (float)i / 60.0f * 2.0f * PI - PI / 2.0f;
		bool major  = (i % 5 == 0);
		float inner = major ? r - 8.0f : r - 4.0f;

		Gfx::Point p1 = { (int)(cx + cosf(angle) * inner),    (int)(cy + sinf(angle) * inner) };
		Gfx::Point p2 = { (int)(cx + cosf(angle) * (float)r), (int)(cy + sinf(angle) * (float)r) };
		draw_line(ctx, p1, p2, major ? COLOR_TICK_MAJOR : COLOR_TICK_MINOR, major);
	}
}

// Gambar angka penunjuk jam utama
void ClockWidget::draw_hour_labels(const UI::DrawContext& ctx) const {
	const int cx = CLOCK_CENTER_X;
	const int cy = CLOCK_CENTER_Y;
	const float label_r = CLOCK_RADIUS - 16.0f;

	struct { int hour; const char* text; } labels[] = {
		{0, "12"}, {3, "3"}, {6, "6"}, {9, "9"}
	};

	for (auto& lbl : labels) {
		float angle = (float)lbl.hour / 12.0f * 2.0f * PI - PI / 2.0f;
		int lx = (int)(cx + cosf(angle) * label_r) - 4;
		int ly = (int)(cy + sinf(angle) * label_r) - 5;
		ctx.draw_text(lbl.text, { lx, ly, 12, 10 },
		              UI::TextAlignment::CENTER, UI::TextAlignment::CENTER,
		              UI::Theme::font(), COLOR_TICK_MAJOR);
	}
}

// Logic menggambar (repaint) widget
void ClockWidget::do_repaint(const UI::DrawContext& ctx) {
	auto now = get_time();

	const int cx = CLOCK_CENTER_X;
	const int cy = CLOCK_CENTER_Y;
	const int r  = CLOCK_RADIUS;

	// Background utama
	ctx.fill({ 0, 0, CLOCK_WIDTH, CLOCK_HEIGHT }, COLOR_CLOCK_BG);

	// Frame dan muka jam
	ctx.fill_ellipse({ cx - r,     cy - r,     r * 2,     r * 2     }, COLOR_CLOCK_FACE_RIM);
	ctx.fill_ellipse({ cx - r + 2, cy - r + 2, r * 2 - 4, r * 2 - 4 }, COLOR_CLOCK_FACE);

	draw_ticks(ctx);
	draw_hour_labels(ctx);

	// Hitung sudut jarum agar pergerakan terlihat halus (smooth)
	float hour_angle   = ((float)(now.hour % 12) + (float)now.minute / 60.0f) / 12.0f * 2.0f * PI;
	float minute_angle = ((float)now.minute       + (float)now.second / 60.0f) / 60.0f * 2.0f * PI;
	float second_angle = (float)now.second / 60.0f * 2.0f * PI;

	draw_hand(ctx, { cx, cy }, hand_endpoint(cx, cy, hour_angle,   r * 0.55f), COLOR_HOUR_HAND,   true);
	draw_hand(ctx, { cx, cy }, hand_endpoint(cx, cy, minute_angle, r * 0.82f), COLOR_MINUTE_HAND, true);
	draw_hand(ctx,
	          hand_endpoint(cx, cy, second_angle + PI, r * 0.20f),
	          hand_endpoint(cx, cy, second_angle,       r * 0.88f),
	          COLOR_SECOND_HAND, false);

	// Detail titik pusat jam
	ctx.fill({ cx - 3, cy - 3, 7, 7 }, COLOR_CLOCK_CENTER);
	ctx.fill({ cx - 2, cy - 2, 5, 5 }, COLOR_SECOND_HAND);
	ctx.fill({ cx - 1, cy - 1, 3, 3 }, COLOR_CLOCK_CENTER);

	// Render teks waktu digital (HH:MM:SS)
	char time_buf[10];
	snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d", now.hour, now.minute, now.second);
	ctx.draw_text(time_buf, { 0, cy * 2 + 7, CLOCK_WIDTH, 14 },
	              UI::TextAlignment::CENTER, UI::TextAlignment::CENTER,
	              UI::Theme::font(), COLOR_TEXT_TIME);

	// Render teks tanggal (DD Mon YYYY)
	char date_buf[18];
	const char* mon = (now.month >= 0 && now.month < 12) ? s_months[now.month] : "???";
	snprintf(date_buf, sizeof(date_buf), "%d %s %d", now.day, mon, now.year);
	ctx.draw_text(date_buf, { 0, cy * 2 + 22, CLOCK_WIDTH, 12 },
	              UI::TextAlignment::CENTER, UI::TextAlignment::CENTER,
	              UI::Theme::font(), COLOR_TEXT_DATE);
}