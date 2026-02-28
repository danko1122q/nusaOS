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

	Copyright (c) Byteduck 2016-2021. All rights reserved.
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

// PERBAIKAN: Gunakan shared_ptr untuk Timer di map agar ownership jelas.
// Sebelumnya map menyimpan raw Timer* sementara set_interval() juga
// mengembalikan Duck::Ptr<Timer> ke pemanggil — dua pemilik untuk satu objek.
// Ketika Duck::Ptr di-destroy, Timer di-delete, tapi map masih punya raw ptr
// ke memori yang sudah freed → use-after-free saat run_while() mengiterasi map.
// Dengan shared_ptr, kedua pemilik berbagi kepemilikan dengan aman.
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
	// yang bisa crash lagi karena state sudah rusak
	_exit(128 + sig);
}

void UI::init(char** argv, char** envp) {
	// Pasang crash handler untuk semua sinyal fatal dari userspace
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
				if (it != windows.end()) {
					// FIX: Cek is_closed() sebelum deregister.
					// Pond recycle window ID — jika event DESTROY masih ada di queue
					// untuk ID lama, tapi ID itu sudah dipakai window baru yang baru
					// saja dibuat, jangan deregister window baru tersebut.
					auto& win = it->second;
					if (!win || win->pond_window() == nullptr || win->is_closed())
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
					if (evt.focused && window->pond_window()->type() != Pond::MENU)
						_last_focused_window = window;
					window->on_focus(evt.focused);
				}
			}
		}
	}
}

void UI::run_while(std::function<bool()> predicate) {
	while (!should_exit && predicate()) {
		// PERBAIKAN: Kumpulkan timer yang siap terpicu ke dalam vector terlebih dahulu,
		// lalu jalankan callback-nya setelah iterasi selesai.
		//
		// Masalah sebelumnya: timer->call()() dipanggil DI DALAM iterasi timers map.
		// Jika callback menyebabkan widget di-destroy → Timer::~Timer() terpanggil
		// → remove_timer() → timers.erase() di dalam iterasi aktif
		// → iterator invalidation → crash/undefined behavior.
		//
		// Fix: pisahkan fase "kumpulkan yang siap" dari fase "jalankan callback".

		long shortest_timeout = LONG_MAX;
		bool have_timeout = false;

		// Fase 1: Tentukan timer mana yang siap, kumpulkan ID-nya
		std::vector<int> ready_ids;
		std::vector<int> timeout_ids;

		for(auto& [id, timer] : timers) {
			if(!timer || !timer->enabled())
				continue;
			long millis = timer->millis_until_ready();
			if(millis <= 0) {
				ready_ids.push_back(id);
			} else {
				if(millis < shortest_timeout) {
					shortest_timeout = millis;
					have_timeout = true;
				}
			}
		}

		// Fase 2: Jalankan callback semua timer yang siap
		// Iterasi by ID (bukan iterator map) agar aman walau map berubah
		for(int id : ready_ids) {
			auto it = timers.find(id);
			if(it == timers.end())
				continue; // timer sudah dihapus oleh callback sebelumnya
			auto timer = it->second; // shared_ptr — keep alive selama callback
			if(!timer || !timer->enabled())
				continue;

			timer->call()(); // panggil callback

			if(!timer->is_interval()) {
				// One-shot timer: hapus setelah dipanggil
				timers.erase(id);
			} else {
				// Interval timer: reschedule jika masih ada di map
				// (bisa sudah dihapus jika widget di-destroy di dalam callback)
				auto it2 = timers.find(id);
				if(it2 != timers.end() && it2->second && it2->second->enabled())
					it2->second->calculate_trigger_time();
			}
		}

		// Hitung ulang shortest_timeout setelah callback mungkin menambah timer baru
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

		update(have_timeout ? shortest_timeout : -1);
	}
}

void UI::run() {
	UI::run_while([] { return true; });
}

void UI::update(int timeout) {
	handle_pond_events();

	for(auto window : windows) {
		if(window.second)
			window.second->repaint_now();
	}

	poll(pollfds.data(), pollfds.size(), timeout);
	for(auto& pollfd : pollfds) {
		if(pollfd.revents) {
			auto& poll = polls[pollfd.fd];
			if(poll.on_ready_to_read && pollfd.revents & POLLIN)
				poll.on_ready_to_read();
			if(poll.on_ready_to_write && pollfd.revents & POLLOUT)
				poll.on_ready_to_write();
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
	// PERBAIKAN: Gunakan shared_ptr agar ownership jelas dan tidak leak
	timers[id] = std::make_shared<Timer>(id, interval, std::move(func), false);
}

Duck::Ptr<Timer> UI::set_interval(std::function<void()> func, int interval) {
	int id = ++cur_timeout;
	// PERBAIKAN: Buat shared_ptr dulu, lalu simpan ke map DAN kembalikan ke pemanggil.
	// Sebelumnya: new Timer() lalu map simpan raw ptr DAN Duck::Ptr dibuat dari ptr yang sama
	// → dua pemilik berbeda untuk satu objek → double-free saat keduanya di-destroy.
	// Sekarang: satu shared_ptr yang di-share antara map dan Duck::Ptr pemanggil.
	auto shared = std::make_shared<Timer>(id, interval, std::move(func), true);
	timers[id] = shared;
	// Duck::Ptr dibungkus dari shared.get() dengan custom deleter yang tidak delete
	// (karena shared_ptr yang manage lifetime-nya)
	return Duck::Ptr<Timer>(shared.get(), [shared_copy = shared](Timer*) mutable {
		// Tidak delete — shared_ptr yang akan handle cleanup saat ref count = 0
		shared_copy.reset();
	});
}

void UI::remove_timer(int id) {
	// Hapus dari map — shared_ptr di map di-release,
	// tapi jika Duck::Ptr pemanggil masih pegang, timer tidak benar-benar di-delete
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
	// FIX: Reset should_exit setiap kali window baru didaftarkan.
	// Tanpa ini: pond recycle window ID → app lama tutup → should_exit=true
	// → app baru buka window dengan ID yang sama → register dipanggil tapi
	// should_exit tidak di-reset → run_while() langsung exit di iterasi pertama.
	should_exit = false;
}

void UI::__deregister_window(int id) {
	windows.erase(id);

	// Jangan exit jika masih ada window DEFAULT, DESKTOP, atau PANEL yang hidup.
	// Window bertipe MENU tidak dihitung — menu bisa dibuka/tutup kapan saja
	// tanpa menyebabkan proses exit.
	for (auto& window : windows) {
		if(!window.second)
			continue;
		auto type = window.second->pond_window()->type();
		if (type == Pond::DEFAULT || type == Pond::DESKTOP || type == Pond::PANEL) {
			should_exit = false;
			return;
		}
	}
	should_exit = true;
}