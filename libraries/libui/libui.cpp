/*
	This file is part of nusaOS.
	SPDX-License-Identifier: GPL-3.0-or-later
	Copyright © 2016-2026 nusaOS
*/

#include "libui.h"
#include "Theme.h"
#include "UIException.h"
#include <poll.h>
#include <map>
#include <utility>
#include <libnusa/Config.h>
#include <climits>
#include <csignal>
#include <cstdio>
#include <unistd.h>

using namespace UI;

Pond::Context* UI::pond_context = nullptr;
std::vector<pollfd> pollfds;
std::map<int, Poll> polls;
std::map<int, std::shared_ptr<Window>> windows;
std::weak_ptr<Window> _last_focused_window;
int cur_timeout = 0;

// FIX #1: Timer map menyimpan shared_ptr<Timer>, bukan raw ptr.
// Sebelumnya map menyimpan raw Timer* sementara set_interval() mengembalikan
// Duck::Ptr<Timer> ke pemanggil — dua pemilik independen untuk satu objek.
// Ketika Duck::Ptr di-destroy, Timer di-delete, tapi map masih punya dangling
// raw ptr → use-after-free saat run_while() mengiterasi map → CRASH.
// Dengan shared_ptr, kepemilikan di-share dengan aman via reference counting.
std::map<int, std::shared_ptr<Timer>> timers;

int num_windows = 0;
bool should_exit = false;
App::Info _app_info;

void handle_pond_events();

// Crash handler — dipasang di UI::init() agar semua app libui mendapat proteksi.
// Saat app crash (SIGSEGV, SIGILL, dll), handler ini disconnect dari Pond
// secara bersih sebelum exit sehingga Pond tidak crash dan desktop tetap stabil.
static void ui_crash_handler(int sig) {
	if(UI::pond_context) {
		for(auto& w : windows) {
			if(w.second && w.second->pond_window())
				w.second->pond_window()->destroy();
		}
	}
	// Gunakan _exit() bukan exit() — tidak memanggil destruktor
	// yang bisa crash lagi karena state sudah rusak.
	_exit(128 + sig);
}

void UI::init(char** argv, char** envp) {
	signal(SIGSEGV, ui_crash_handler);
	signal(SIGILL,  ui_crash_handler);
	signal(SIGABRT, ui_crash_handler);
	signal(SIGBUS,  ui_crash_handler);
	signal(SIGFPE,  ui_crash_handler);

	pond_context = Pond::Context::init();
	pollfds.clear();

	auto app_res = App::Info::from_current_app();
	if(app_res.has_value()) {
		_app_info = app_res.value();
		pond_context->set_app_info(_app_info);
	}

	Poll pond_poll = {pond_context->connection_fd()};
	pond_poll.on_ready_to_read = handle_pond_events;
	add_poll(pond_poll);

	auto cfg_res = Duck::Config::read_from("/etc/libui.conf");
	if(!cfg_res.is_error()) {
		auto& cfg = cfg_res.value();
		if(cfg.has_section("theme"))
			Theme::load_config(cfg["theme"]);
	}

	Theme::current();
}

Duck::Ptr<Window> find_window(int id) {
	auto it = windows.find(id);
	if(it == windows.end())
		return nullptr;
	return it->second;
}

void handle_pond_events() {
	while(UI::pond_context->has_event()) {
		Pond::Event event = UI::pond_context->next_event();
		switch(event.type) {
			case PEVENT_KEY: {
				auto& evt = event.key;
				auto window = find_window(evt.window->id());
				if(window)
					window->on_keyboard(evt);
				break;
			}

			case PEVENT_MOUSE_MOVE: {
				auto& evt = event.mouse_move;
				auto window = find_window(evt.window->id());
				if(window)
					window->on_mouse_move(evt);
				break;
			}

			case PEVENT_MOUSE_BUTTON: {
				auto& evt = event.mouse_button;
				auto window = find_window(evt.window->id());
				if(window)
					window->on_mouse_button(evt);
				break;
			}

			case PEVENT_MOUSE_SCROLL: {
				auto& evt = event.mouse_scroll;
				auto window = find_window(evt.window->id());
				if(window)
					window->on_mouse_scroll(evt);
				break;
			}

			case PEVENT_MOUSE_LEAVE: {
				auto& evt = event.mouse_leave;
				auto window = find_window(evt.window->id());
				if(window)
					window->on_mouse_leave(evt);
				break;
			}

			case PEVENT_WINDOW_DESTROY: {
				auto it = windows.find(event.window_destroy.id);
				if(it != windows.end()) {
					auto win = it->second; // shared_ptr — keep alive
					if(win) {
						// Tandai bahwa Pond sudah destroy window ini, sehingga
						// Window::close() tidak memanggil _window->destroy() lagi.
						win->mark_pond_destroyed();
						if(!win->is_closed())
							win->close(); // Trigger on_close callback, set _closed = true
					}
					__deregister_window(event.window_destroy.id);
				}
				break;
			}

			case PEVENT_WINDOW_RESIZE: {
				auto& evt = event.window_resize;
				auto window = find_window(evt.window->id());
				if(window) {
					window->on_resize(evt.old_rect);
					window->repaint();
				}
				break;
			}

			case PEVENT_WINDOW_FOCUS: {
				auto& evt = event.window_focus;
				auto window = find_window(evt.window->id());
				if(window) {
					if(evt.focused && window->pond_window()->type() != Pond::MENU)
						_last_focused_window = window;
					window->on_focus(evt.focused);
				}
				break;
			}
		}
	}
}

void UI::run_while(std::function<bool()> predicate) {
	while(!should_exit && predicate()) {
		// FIX #3: Iterator invalidation — timer callback merusak map saat iterasi.
		//
		// Masalah lama: timer->call()() dipanggil DI DALAM for(auto& [id, timer] : timers).
		// Jika callback menghancurkan widget → Timer destruktor terpanggil
		// → remove_timer() → timers.erase() di tengah iterasi aktif
		// → iterator invalidation → undefined behavior / crash.
		//
		// FIX: Pisahkan menjadi dua fase:
		//   Fase 1 — kumpulkan ID timer yang siap ke dalam vector (tidak modify map)
		//   Fase 2 — eksekusi callback by ID (map mungkin berubah, tapi kita lookup ulang)
		//
		// Gunakan shared_ptr untuk "keep alive" timer selama callback berjalan,
		// sehingga walaupun map entry di-erase di dalam callback, objek Timer
		// tidak langsung di-delete dan dangling reference tidak terjadi.

		// Fase 1: Kumpulkan ID timer yang sudah siap
		std::vector<int> ready_ids;
		long shortest_timeout = LONG_MAX;
		bool have_timeout = false;

		for(auto& [id, timer] : timers) {
			if(!timer || !timer->enabled())
				continue;
			long millis = timer->millis_until_ready();
			if(millis <= 0) {
				ready_ids.push_back(id);
			} else if(millis < shortest_timeout) {
				shortest_timeout = millis;
				have_timeout = true;
			}
		}

		// Fase 2: Eksekusi callback — aman dari iterator invalidation
		for(int id : ready_ids) {
			// Lookup ulang — timer mungkin sudah dihapus oleh callback sebelumnya
			auto it = timers.find(id);
			if(it == timers.end())
				continue;

			// Pegang shared_ptr agar timer tidak langsung di-delete jika map entry
			// dihapus di dalam callback (misal widget tutup → ~Timer → remove_timer)
			auto timer = it->second;
			if(!timer || !timer->enabled())
				continue;

			timer->call()(); // Panggil callback — map BOLEH berubah di sini

			// Setelah callback: cek apakah timer masih ada di map
			auto it2 = timers.find(id);
			if(it2 == timers.end())
				continue; // Sudah dihapus oleh callback, skip reschedule

			if(!timer->is_interval()) {
				// One-shot: hapus setelah dipanggil
				timers.erase(id);
			} else if(timer->enabled()) {
				// Interval: reschedule
				timer->calculate_trigger_time();
			}
		}

		// Hitung ulang timeout setelah callback mungkin menambah/hapus timer
		shortest_timeout = LONG_MAX;
		have_timeout = false;
		for(auto& [id, timer] : timers) {
			if(!timer || !timer->enabled())
				continue;
			long millis = timer->millis_until_ready();
			if(millis > 0 && millis < shortest_timeout) {
				shortest_timeout = millis;
				have_timeout = true;
			}
		}

		update(have_timeout ? (int)shortest_timeout : -1);
	}
}

void UI::run() {
	UI::run_while([] { return true; });
}

void UI::update(int timeout) {
	handle_pond_events();

	for(auto& window : windows) {
		if(window.second)
			window.second->repaint_now();
	}

	poll(pollfds.data(), pollfds.size(), timeout);
	for(auto& pfd : pollfds) {
		if(pfd.revents) {
			auto& p = polls[pfd.fd];
			if(p.on_ready_to_read && pfd.revents & POLLIN)
				p.on_ready_to_read();
			if(p.on_ready_to_write && pfd.revents & POLLOUT)
				p.on_ready_to_write();
		}
	}
}

bool UI::ready_to_exit() {
	return should_exit;
}

Duck::WeakPtr<Window> UI::last_focused_window() {
	return _last_focused_window;
}

void UI::set_timeout(std::function<void()> func, int interval) {
	int id = ++cur_timeout;
	timers[id] = std::make_shared<Timer>(id, interval, std::move(func), false);
}

Duck::Ptr<Timer> UI::set_interval(std::function<void()> func, int interval) {
	int id = ++cur_timeout;

	// FIX #4: set_interval() double-ownership / double-free.
	//
	// Masalah lama:
	//   auto* raw = new Timer(...);
	//   timers[id] = raw;                    // map punya raw ptr
	//   return Duck::Ptr<Timer>(raw);         // Duck::Ptr juga punya raw ptr
	// Ketika Duck::Ptr di-destroy → delete raw. Map masih pegang dangling raw ptr.
	// Ketika timers di-iterate → use-after-free → CRASH.
	// Ketika Timer::~Timer() → remove_timer() → double erase (ok tapi confusing).
	//
	// FIX: Buat satu shared_ptr, simpan di map DAN wrap ke Duck::Ptr.
	// Duck::Ptr menggunakan custom deleter yang me-reset shared_ptr-nya sendiri
	// (melepas referensi Duck::Ptr), sehingga Timer hanya benar-benar di-delete
	// setelah SEMUA pemilik (map + Duck::Ptr) melepasnya.
	auto shared = std::make_shared<Timer>(id, interval, std::move(func), true);
	timers[id] = shared;

	// Duck::Ptr membawa shared_ptr copy — saat Duck::Ptr di-destroy,
	// hanya ref count shared_ptr yang turun; delete terjadi hanya jika ref count = 0.
	return Duck::Ptr<Timer>(shared.get(), [owned = shared](Timer*) mutable {
		owned.reset(); // Lepas referensi ini; map mungkin masih pegang satu lagi
	});
}

void UI::remove_timer(int id) {
	// Hapus dari map. Jika Duck::Ptr pemanggil masih hidup, shared_ptr di dalamnya
	// masih pegang referensi dan Timer belum benar-benar di-delete.
	timers.erase(id);
}

bool UI::set_app_name(const std::string& app_name) {
	auto app_res = App::Info::from_app_name(app_name);
	if(!app_res.is_error()) {
		_app_info = app_res.value();
		pond_context->set_app_info(_app_info);
	}
	return !app_res.is_error();
}

App::Info& UI::app_info() {
	return _app_info;
}

void UI::add_poll(const Poll& poll) {
	if(!poll.on_ready_to_read && !poll.on_ready_to_write)
		return;
	pollfd pfd = {.fd = poll.fd, .events = 0, .revents = 0};
	if(poll.on_ready_to_read)
		pfd.events |= POLLIN;
	if(poll.on_ready_to_write)
		pfd.events |= POLLOUT;
	polls[poll.fd] = poll;
	pollfds.push_back(pfd);
}

Duck::Ptr<const Gfx::Image> UI::icon(Duck::Path path) {
	if(path.is_absolute())
		return _app_info.resource_image("/usr/share/icons" + path.string() + (path.extension().empty() ? ".icon" : ""));
	return _app_info.resource_image(path);
}

void UI::__register_window(const std::shared_ptr<Window>& window, int id) {
	windows[id] = window;
	// Reset should_exit setiap kali window baru didaftarkan.
	// Tanpa ini: Pond recycle window ID → app lama tutup → should_exit=true
	// → app baru buka window dengan ID yang sama → run_while() langsung exit.
	should_exit = false;
}

void UI::__deregister_window(int id) {
	windows.erase(id);

	// Exit jika tidak ada lagi "main window" yang tersisa.
	// Window MENU (dari static pool di MenuWidget) tidak dihitung — mereka
	// hidup sepanjang umur app dan tidak pernah di-destroy.
	// Window yang di-mark_as_menu_window() saat acquire juga tidak dihitung.
	//
	// Sebelumnya kode ini akses pond_window()->type() untuk cek tipe window —
	// tapi pond_window() bisa dangling setelah window di-destroy → crash.
	// Sekarang kita gunakan flag lokal yang aman dibaca kapan saja.
	for(auto& [wid, win] : windows) {
		if(win && !win->is_menu_window())
			return; // Masih ada main window aktif, jangan exit
	}
	should_exit = true;
}