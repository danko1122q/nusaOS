/*
	This file is part of nusaOS.
	SPDX-License-Identifier: GPL-3.0-or-later
	Copyright © 2016-2026 nusaOS
*/

#include "Timer.h"
#include "libui.h"

using namespace UI;

Timer::Timer() = default;

Timer::Timer(int id, int delay, std::function<void()> call, bool is_interval):
	m_id(id), m_delay(delay), m_call(std::move(call)), m_interval(is_interval)
{
	calculate_trigger_time();
}

Timer::~Timer() {
	// FIX: Destruktor Timer TIDAK boleh memanggil remove_timer() secara langsung.
	//
	// Alur crash lama:
	//   run_while() iterasi timers map
	//   → timer->call()() dipanggil
	//   → callback menghancurkan widget
	//   → ~ProcessInspectorWidget() → m_timer (Duck::Ptr) di-destroy
	//   → ~Timer() → UI::remove_timer(m_id) → timers.erase(m_id)
	//   → erase di dalam iterasi aktif → iterator invalidation → CRASH
	//
	// Dengan shared_ptr di map (libui.cpp fix), Timer::~Timer() hanya dipanggil
	// setelah ref count benar-benar nol, yaitu SETELAH map entry sudah di-erase
	// (oleh run_while fase reschedule atau oleh callback stop()/remove_timer()).
	// Jadi remove_timer() di sini tidak diperlukan lagi dan justru berbahaya
	// jika dipanggil saat iterasi masih aktif.
	//
	// Timer yang perlu dibersihkan dari map ditangani oleh:
	//   - run_while(): erase one-shot timer setelah dipanggil
	//   - Timer::stop() + run_while(): skip disabled timer, dibersihkan lazily
	//   - UI::remove_timer(): dipanggil eksplisit jika perlu hapus segera
	//
	// Destruktor ini sengaja kosong.
}

void Timer::calculate_trigger_time() {
	auto now = Duck::Time::now();
	if(!m_trigger_time.epoch())
		m_trigger_time = Duck::Time::now();
	while(m_trigger_time <= now)
		m_trigger_time = m_trigger_time + Duck::Time::millis(m_delay);
}

bool Timer::ready() const {
	return millis_until_ready() <= 0;
}

long Timer::millis_until_ready() const {
	return (m_trigger_time - Duck::Time::now()).millis();
}

void Timer::stop() {
	m_enabled = false;
}

void Timer::start() {
	m_enabled = true;
	calculate_trigger_time();
}