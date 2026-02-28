/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2023 Byteduck */

#include "ViewerAudioWidget.h"
#include <libsound/Sound.h>
#include <libui/libui.h>
#include <libui/widget/Cell.h>

ViewerAudioWidget::ViewerAudioWidget(Duck::Ptr<Sound::WavReader> reader): UI::BoxLayout(UI::BoxLayout::HORIZONTAL) {
	Sound::init();
	m_source = Sound::add_source(reader);
	set_spacing(2);

	m_play_button = UI::Button::make("||");
	m_play_button->on_pressed = [this] {
		if(std::abs(m_source->total_time() - m_source->cur_time()) < 0.001)
			m_source->seek(0.0);
		m_source->set_playing(!m_source->playing());
	};

	m_stop_button = UI::Button::make("[]");
	m_stop_button->on_pressed = [this] {
		m_source->set_playing(false);
		m_source->seek(0);
		update();
	};

	m_ff_button = UI::Button::make(">>");
	m_ff_button->on_pressed = [this] {
		m_source->seek(m_source->cur_time() + 10.0f);
		update();
	};

	m_rev_button = UI::Button::make("<<");
	m_rev_button->on_pressed = [this] {
		m_source->seek(m_source->cur_time() - 10.0f);
		update();
	};

	m_progress_bar = UI::ProgressBar::make();
	m_time_label = UI::Label::make("0:00");

	add_child(UI::Cell::make(m_time_label));
	add_child(m_progress_bar);
	add_child(m_stop_button);
	add_child(m_play_button);
	add_child(m_rev_button);
	add_child(m_ff_button);

	m_timer = UI::set_interval([this] {
		update();
	}, 1000 / 60);
}

ViewerAudioWidget::~ViewerAudioWidget() {
	// WAJIB stop timer sebelum objek di-destroy.
	// Tanpa ini: timer 60fps terus fire setelah widget di-destroy →
	// callback [this] { update(); } akses m_source/m_progress_bar yang sudah freed
	// → use-after-free → BSOD saat WAV viewer ditutup.
	if (m_timer)
		m_timer->stop();
}

void ViewerAudioWidget::update() {
	float cur_time = m_source->cur_time();
	int seconds = (int) cur_time % 60;
	int minutes = (int) cur_time / 60;
	m_time_label->set_label(std::to_string(minutes) + ":" + (seconds < 10 ? "0" : "") + std::to_string(seconds));
	// Division by zero guard: total_time() == 0 jika WAV kosong/rusak.
	// set_progress(NaN) → undefined behavior di progress bar draw.
	auto total = m_source->total_time();
	m_progress_bar->set_progress(total > 0.0f ? cur_time / total : 0.0f);
	m_play_button->set_label(m_source->playing() ? "||" : "|>");
}