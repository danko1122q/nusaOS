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
	Copyright (c) ChazizGRKB 2022.
*/

#include "Display.h"
#include "FontManager.h"
#include <libgraphics/Image.h>
#include <libgraphics/PNG.h>
#include <libnusa/Log.h>
#include <libnusa/Config.h>
#include <unistd.h>
#include <cstdio>
#include <sys/ioctl.h>
#include <kernel/device/VGADevice.h>
#include <sys/input.h>
#include <libgraphics/Memory.h>

using namespace Gfx;
using Duck::Log, Duck::Config, Duck::ResultRet;

Display* Display::_inst = nullptr;

Display::Display(): _dimensions({0, 0, 0, 0}) {
	_inst = this;
	framebuffer_fd = open("/dev/fb0", O_RDWR | O_CLOEXEC);
	if(framebuffer_fd < -1) {
		perror("Failed to open framebuffer");
		return;
	}

	if(ioctl(framebuffer_fd, IO_VIDEO_WIDTH, &_dimensions.width) < 0) {
		perror("Failed to get framebuffer width");
		return;
	}

	if(ioctl(framebuffer_fd, IO_VIDEO_HEIGHT, &_dimensions.height) < 0) {
		perror("Failed to get framebuffer height");
		return;
	}

	Gfx::Color* buffer;

	if(ioctl(framebuffer_fd, IO_VIDEO_MAP, &buffer) < 0) {
		perror("Failed to map framebuffer");
		return;
	}

	// Set tty ke graphical mode jika berjalan di tty
	ioctl(STDOUT_FILENO, TIOSGFX, nullptr);

	// Jika bisa set offset ke video memory, gunakan double-flip buffer
	if(!ioctl(framebuffer_fd, IO_VIDEO_OFFSET, 0))
		_buffer_mode = BufferMode::DoubleFlip;
	else
		_buffer_mode = BufferMode::Double;

	_framebuffer = {buffer, _dimensions.width, _dimensions.height};
	Log::info("Display opened and mapped (", _dimensions.width, " x ", _dimensions.height, ")");

	if((_keyboard_fd = open("/dev/input/keyboard", O_RDONLY | O_CLOEXEC)) < 0)
		perror("Failed to open keyboard");

	gettimeofday(&paint_time, NULL);
}

Gfx::Rect Display::dimensions() {
	return _dimensions;
}

Framebuffer& Display::framebuffer() {
	return _framebuffer;
}

void Display::clear(uint32_t color) {
	size_t framebuffer_size = _dimensions.area();
	for(size_t i = 0; i < framebuffer_size; i++) {
		_framebuffer.data[i] = color;
	}
}

Duck::Result Display::load_config() {
	auto cfg = TRY(Duck::Config::read_from("/etc/pond.conf"));
	if(cfg.has_section("desktop")) {
		auto desktop = cfg.section("desktop");
		if(!desktop["background"].empty()) {
			int num = sscanf(desktop["background"].c_str(), "#%lx , #%lx", &_background_a, &_background_b);
			if(!num) {
				auto wallpaper_res = Gfx::Image::load(desktop["background"]);
				if(wallpaper_res.has_value())
					_wallpaper = wallpaper_res.value();
			} else if(num == 1) {
				_background_b = _background_a;
			}
		}
	}
	return Duck::Result::SUCCESS;
}

void Display::set_root_window(Window* window) {
	_root_window = window;
	load_config();
	_root_window->invalidate();
	_background_framebuffer = Framebuffer(_root_window->rect().width, _root_window->rect().height);
	auto& fb = _background_framebuffer;
	if(_wallpaper)
		_wallpaper->draw(fb, {0, 0, fb.width, fb.height});
	else
		fb.fill_gradient_v({0, 0, fb.width, fb.height}, _background_a, _background_b);
}

Window* Display::root_window() {
	return _root_window;
}

void Display::set_mouse_window(Mouse* window) {
	_mouse_window = window;
}

Mouse* Display::mouse_window() {
	return _mouse_window;
}

void Display::add_window(Window* window) {
	_windows.push_back(window);
}

void Display::remove_window(Window* window) {
	if(window == _focused_window)
		_focused_window = nullptr;
	if(window == _prev_mouse_window)
		_prev_mouse_window = nullptr;
	if(window == _drag_window)
		_drag_window = nullptr;
	if(window == _resize_window)
		_resize_window = nullptr;
	if(window == _mousedown_window)
		_mousedown_window = nullptr;
	for(size_t i = 0; i < _windows.size(); i++) {
		if(_windows[i] == window) {
			_windows.erase(_windows.begin() + i);
			return;
		}
	}
}

void Display::invalidate(const Gfx::Rect& rect) {
	if(!rect.empty())
		invalid_areas.push_back(rect);
}

//#define DEBUG_REPAINT_PERF
void Display::repaint() {
#ifdef DEBUG_REPAINT_PERF
	timeval t0, t1;
	gettimeofday(&t0, nullptr);
#endif

	if(!invalid_areas.empty())
		display_buffer_dirty = true;
	else
		return;

	// Jika belum 1/60 detik sejak repaint terakhir, skip
	if(millis_until_next_flip())
		return;
	gettimeofday(&paint_time, NULL);

	// Saat resize, selalu invalidate resize rect agar outline invert tetap benar
	if(_resize_window)
		invalidate(_resize_rect);

	auto& fb = _buffer_mode == BufferMode::Single ? _framebuffer : _root_window->framebuffer();

	// Gabungkan area yang overlap
	// Gunakan return value erase() untuk mendapat iterator valid berikutnya,
	// bukan erase(it) lalu it++ yang menyebabkan dangling iterator
	auto it = invalid_areas.begin();
	while(it != invalid_areas.end()) {
		bool remove_area = false;
		for(auto& other_area : invalid_areas) {
			if(&*it != &other_area && it->collides(other_area)) {
				other_area = it->combine(other_area);
				remove_area = true;
				break;
			}
		}
		if(remove_area)
			it = invalid_areas.erase(it);
		else
			++it;
	}

	// Jika double buffering, gabungkan area untuk menghitung bagian framebuffer yang perlu digambar ulang
	if(_buffer_mode == BufferMode::Double) {
		if(_invalid_buffer_area.x == -1)
			_invalid_buffer_area = invalid_areas[0];
		for(auto& area : invalid_areas)
			_invalid_buffer_area = _invalid_buffer_area.combine(area);
	}

	for(auto& area : invalid_areas) {
		// Isi area invalid dengan background
		fb.copy(_background_framebuffer, area, area.position());

		// Render window DESKTOP lebih dulu agar selalu berada di layer paling bawah
		for(auto window : _windows) {
			if(window->type() != Pond::DESKTOP || window->hidden())
				continue;
			Gfx::Rect window_abs = window->absolute_rect();
			Gfx::Rect overlap_abs = area.overlapping_area(window_abs);
			if(overlap_abs.empty())
				continue;
			auto transformed_overlap = overlap_abs.transform({-window_abs.x, -window_abs.y});
			if(window->uses_alpha())
				fb.copy_blitting(window->framebuffer(), transformed_overlap, overlap_abs.position());
			else
				fb.copy(window->framebuffer(), transformed_overlap, overlap_abs.position());
		}

		// Render semua window lainnya (bukan mouse window, bukan hidden, bukan DESKTOP)
		for(auto window : _windows) {
			if(window == _mouse_window || window->hidden() || window->type() == Pond::DESKTOP)
				continue;

			auto window_old_rect = window->old_absolute_shadow_rect();
			auto window_shabs = window->absolute_shadow_rect();
			auto window_collision_rect = window_old_rect.empty() ? window_shabs : window_old_rect;
			if(window_collision_rect.collides(area)) {
				Gfx::Rect window_abs = window->absolute_rect();
				Gfx::Rect overlap_abs = area.overlapping_area(window_abs);
				if(!overlap_abs.empty()) {
					auto transformed_overlap = overlap_abs.transform({-window_abs.x, -window_abs.y});
					if(window->uses_alpha())
						fb.copy_blitting(window->framebuffer(), transformed_overlap, overlap_abs.position());
					else
						fb.copy(window->framebuffer(), transformed_overlap, overlap_abs.position());

					// Jika client tidak responsif, redupkan window
					// Guard null check untuk window tanpa client (root window, mouse window)
					if(window->client() && window->client()->is_unresponsive())
						fb.fill_blitting(overlap_abs, {0, 0, 0, 180});
				}

				// Gambar shadow (rect sendiri, di luar guard overlap_abs window)
				if(window->has_shadow()) {
					auto draw_shadow = [&](Gfx::Framebuffer& shadow_buffer, Rect rect) {
						Gfx::Rect shadow_abs = area.overlapping_area(rect);
						if(shadow_abs.empty())
							return;
						fb.copy_blitting(shadow_buffer, shadow_abs.transform(rect.position() * -1), shadow_abs.position());
					};

					auto shadow_size = window_abs.x - window_shabs.x;
					draw_shadow(window->shadow_buffers()[0], window_shabs.inset(0, 0, window_shabs.height - shadow_size, 0));
					draw_shadow(window->shadow_buffers()[1], window_shabs.inset(window_shabs.height - shadow_size, 0, 0, 0));
					draw_shadow(window->shadow_buffers()[2], window_shabs.inset(shadow_size, window_shabs.width - shadow_size, shadow_size, 0));
					draw_shadow(window->shadow_buffers()[3], window_shabs.inset(shadow_size, 0, shadow_size, window_shabs.width - shadow_size));
				}
			}
		}
	}
	invalid_areas.resize(0);

	// Gambar outline saat resize
	if(_resize_window)
		fb.outline_inverting_checkered(_resize_rect);

	// Gambar mouse cursor — guard null untuk mencegah crash jika mouse window belum siap
	if(_mouse_window)
		fb.draw_image(_mouse_window->framebuffer(), {0, 0, _mouse_window->rect().width, _mouse_window->rect().height},
				  _mouse_window->absolute_rect().position());

#ifdef DEBUG_REPAINT_PERF
	timeval t1;
	char buf[10];
	t1.tv_sec -= t0.tv_sec;
	t1.tv_usec -= t0.tv_usec;
	if(t1.tv_usec < 0) {
		t1.tv_sec -= 1 + t1.tv_usec / -1000000;
		t1.tv_usec = (1000000 - (-t1.tv_usec % 1000000)) % 1000000;
	}
	snprintf(buf, 10, "%dms", (int)(t1.tv_usec / 1000 + t1.tv_sec * 1000));
	fb.fill({0, 0, 50, 14}, RGB(0, 0, 0));
	fb.draw_text(buf, {0, 0}, FontManager::inst().get_font("gohu-14"), RGB(255, 255, 255));
#endif

	flip_buffers();
}

bool flipped = false;
void Display::flip_buffers() {
	if(!display_buffer_dirty)
		return;

	if(_buffer_mode == BufferMode::DoubleFlip) {
		auto* video_buf = &_framebuffer.data[flipped ? _framebuffer.height * _framebuffer.width : 0];
		memcpy_uint32((uint32_t*) video_buf, (uint32_t*) _root_window->framebuffer().data, _framebuffer.width * _framebuffer.height);
		ioctl(framebuffer_fd, IO_VIDEO_OFFSET, flipped ? _framebuffer.height : 0);
		flipped = !flipped;
	} else if(_buffer_mode == BufferMode::Double) {
		_framebuffer.copy(_root_window->framebuffer(), _invalid_buffer_area, _invalid_buffer_area.position());
		_invalid_buffer_area.x = -1;
	}

	display_buffer_dirty = false;
}

int Display::millis_until_next_flip() const {
	timeval new_time = {0, 0};
	gettimeofday(&new_time, NULL);
	int diff = (int) (((new_time.tv_sec - paint_time.tv_sec) * 1000000) + (new_time.tv_usec - paint_time.tv_usec))/1000;
	return diff >= 16 ? 0 : 16 - diff;
}

void Display::move_to_front(Window* window) {
	// Layering dari bawah ke atas: DESKTOP → DEFAULT → PANEL → MENU
	// DESKTOP tidak boleh naik sama sekali
	if(window->type() == Pond::DESKTOP)
		return;

	for(auto it = _windows.begin(); it != _windows.end(); it++) {
		if(*it != window)
			continue;
		_windows.erase(it);

		if(window->type() == Pond::MENU) {
			// MENU selalu paling atas
			_windows.push_back(window);
		} else if(window->type() == Pond::PANEL) {
			// PANEL di atas DEFAULT tapi di bawah MENU
			auto insert_pos = _windows.end();
			for(auto jt = _windows.begin(); jt != _windows.end(); jt++) {
				if((*jt)->type() == Pond::MENU) {
					insert_pos = jt;
					break;
				}
			}
			_windows.insert(insert_pos, window);
		} else {
			// DEFAULT: sisipkan sebelum window PANEL atau MENU pertama
			auto insert_pos = _windows.end();
			for(auto jt = _windows.begin(); jt != _windows.end(); jt++) {
				if((*jt)->type() == Pond::PANEL || (*jt)->type() == Pond::MENU) {
					insert_pos = jt;
					break;
				}
			}
			_windows.insert(insert_pos, window);
		}

		window->invalidate();
		return;
	}
}

void Display::focus(Window* window) {
	// DESKTOP tidak boleh di-focus
	if(window && window->type() == Pond::DESKTOP)
		return;

	if(window == _focused_window)
		return;

	auto old_focused = _focused_window;
	_focused_window = window;

	// Guard client sebelum notify_focus:
	// window baru mungkin belum set_client(), atau client sedang disconnect
	if(_focused_window && _focused_window->client() && !_focused_window->client()->is_unresponsive())
		_focused_window->notify_focus(true);

	if(old_focused && old_focused->client() && !old_focused->client()->is_unresponsive()) {
		bool keep_focused = (_focused_window != nullptr && old_focused == _focused_window->menu_parent());
		if(!keep_focused)
			old_focused->notify_focus(false);
	}
}

void Display::create_mouse_events(int delta_x, int delta_y, int scroll, uint8_t buttons) {
	// _prev_mouse_buttons adalah member variable (bukan static local) agar
	// di-reset dengan benar saat Display di-reinit setelah desktop crash

	// Guard: jika mouse window belum siap, tidak ada yang bisa diproses
	if(!_mouse_window)
		return;

	Gfx::Point mouse = _mouse_window->absolute_rect().position();
	Gfx::Point delta = {delta_x, delta_y};

	// Drag window
	if(_drag_window) {
		if(!(buttons & 1) || !_drag_window->draggable())
			_drag_window = nullptr;
		else
			_drag_window->set_position(_drag_window->rect().position() + delta);
	}

	// Resize window
	if(_resize_window) {
		if(!(delta_x == 0 && delta_y == 0)) {
			invalidate(_resize_rect);
			_resize_rect = _resize_window->calculate_absolute_rect(calculate_resize_rect());
		}
		if(!_resize_window->resizable()) {
			_resize_window = nullptr;
		} else if(!(buttons & 1)) {
			_resize_window->set_rect(_resize_rect, true);
			_resize_window = nullptr;
		}
	}

	// Global mouse events
	for(auto& window : _windows) {
		if(window->gets_global_mouse()) {
			window->mouse_moved(delta, mouse - window->absolute_rect().position(), mouse);
			if(buttons != _prev_mouse_buttons)
				window->set_mouse_buttons(buttons);
			if(scroll)
				window->mouse_scrolled(scroll);
		}
	}

	// Jika sedang drag/resize, tidak perlu proses event lainnya
	if(_resize_window || _drag_window)
		return;

	// Jika mousedown window dan tombol dilepas, berhenti kirim event ke sana
	if(_mousedown_window && !(buttons & 1)) {
		if(!mouse.in(_mousedown_window->absolute_rect()))
			_mousedown_window->set_mouse_buttons(buttons);
		_mousedown_window = nullptr;
	}

	// Jika mouse masih ditekan, terus kirim event ke window asal klik
	if(_mousedown_window && !_mousedown_window->gets_global_mouse() && !mouse.in(_mousedown_window->absolute_rect())) {
		_mousedown_window->mouse_moved(delta, mouse - _mousedown_window->absolute_rect().position(), mouse);
		if(_prev_mouse_buttons != buttons)
			_mousedown_window->set_mouse_buttons(buttons);
		if(scroll)
			_mousedown_window->mouse_scrolled(scroll);
		return;
	}

	Window* event_window = nullptr;
	bool found_border_window = false;

	// Pre-pass: tentukan window paling atas yang benar-benar berisi posisi mouse
	// Ini mencegah border window tertutup men-trigger resize cursor
	Window* top_mouse_window = nullptr;
	for(auto it = _windows.rbegin(); it != _windows.rend(); it++) {
		auto* window = *it;
		if(window == _mouse_window || window == _root_window || window->hidden())
			continue;
		if(window->type() == Pond::DESKTOP)
			continue;
		if(mouse.in(window->absolute_rect())) {
			auto window_rel_pos = mouse - window->absolute_rect().position();
			if(window->alpha_hit_testing() && window->framebuffer().at(window_rel_pos)->a == 0)
				continue;
			top_mouse_window = window;
			break;
		}
	}

	for(auto it = _windows.rbegin(); it != _windows.rend(); it++) {
		auto* window = *it;
		if(window == _mouse_window || window == _root_window || window->hidden())
			continue;

		// Cek resize border hanya untuk window paling atas yang berisi mouse
		if(!_resize_window && window->resizable() && window != _mousedown_window &&
		   top_mouse_window == window &&
		   mouse.near_border(window->absolute_rect(), WINDOW_RESIZE_BORDER)) {
			found_border_window = true;
			_resize_mode = get_resize_mode(window->absolute_rect(), mouse);
			switch(_resize_mode) {
				case NORTH:
				case SOUTH:
					_mouse_window->set_cursor(Pond::RESIZE_V);
					break;
				case EAST:
				case WEST:
					_mouse_window->set_cursor(Pond::RESIZE_H);
					break;
				case NORTHWEST:
				case SOUTHEAST:
					_mouse_window->set_cursor(Pond::RESIZE_DR);
					break;
				case NORTHEAST:
				case SOUTHWEST:
					_mouse_window->set_cursor(Pond::RESIZE_DL);
					break;
				default:
					_mouse_window->set_cursor(Pond::NORMAL);
			}

			if(!(_prev_mouse_buttons & 1) && (buttons & 1) && _resize_mode != NONE) {
				_resize_window = window;
				_resize_begin_point = mouse;
				_resize_rect = window->absolute_rect();
				window->move_to_front();
			}
			break;
		}

		// Jika mouse di dalam window, kirim event yang sesuai
		if(mouse.in(window->absolute_rect())) {
			auto window_rel_pos = mouse - window->absolute_rect().position();

			// Alpha hit testing: lewati pixel transparan
			if(window->alpha_hit_testing() && (window->framebuffer().at(window_rel_pos)->a == 0))
				continue;

			// Mouse di dalam window tapi bukan di border: reset kursor ke NORMAL
			if(!found_border_window && window->type() != Pond::DESKTOP)
				_mouse_window->set_cursor(Pond::NORMAL);

			if(window->type() != Pond::DESKTOP)
				event_window = window;
			if(!window->gets_global_mouse()) {
				window->mouse_moved(delta, window_rel_pos, mouse);
				if(_prev_mouse_buttons != buttons)
					window->set_mouse_buttons(buttons);
				if(scroll)
					window->mouse_scrolled(scroll);
			}

			if(!(_prev_mouse_buttons & 1) && (buttons & 1)) {
				if(window->type() == Pond::DESKTOP) {
					// DESKTOP tidak boleh jadi _mousedown_window
				} else {
					window->focus();
					_mousedown_window = window;
					if(window->draggable()) {
						_drag_window = window;
						window->move_to_front();
					}
				}
			}
			if(window->type() == Pond::DESKTOP)
				continue;
			break;
		}
	}

	// Jika mouse tidak di atas window manapun dan tidak di border, reset kursor
	if(!found_border_window && event_window == nullptr && !_resize_window)
		_mouse_window->set_cursor(Pond::NORMAL);

	// Jika mouse pindah dari window sebelumnya, update posisi dan kirim mouse_left
	if(event_window != _prev_mouse_window && _prev_mouse_window != nullptr && !_prev_mouse_window->gets_global_mouse()) {
		Gfx::Dimensions window_dims = _prev_mouse_window->rect().dimensions();
		Gfx::Point new_mouse_pos = (mouse - _prev_mouse_window->absolute_rect().position()).constrain({0, 0, window_dims.width, window_dims.height});
		_prev_mouse_window->mouse_moved(delta, new_mouse_pos, new_mouse_pos + _prev_mouse_window->absolute_rect().position());
		_prev_mouse_window->set_mouse_buttons(_mouse_window->mouse_buttons());
		_prev_mouse_window->mouse_left();
	}

	_prev_mouse_window = event_window;
	_prev_mouse_buttons = buttons;
}

bool Display::buffer_is_dirty() {
	return display_buffer_dirty;
}

bool Display::update_keyboard() {
	KeyboardEvent events[32];
	ssize_t nread = read(_keyboard_fd, &events, sizeof(KeyboardEvent) * 32);
	if(!nread) return false;
	int num_events = (int) nread / sizeof(KeyboardEvent);
	if(_focused_window) {
		for(int i = 0; i < num_events; i++) {
			_focused_window->handle_keyboard_event(events[i]);
		}
	}
	return true;
}

int Display::keyboard_fd() {
	return _keyboard_fd;
}

void Display::window_hidden(Window* window) {
	// Clear semua pointer yang mungkin menunjuk ke window yang disembunyikan
	// agar tidak terjadi akses ke window hidden/destroyed di frame berikutnya
	//
	// CATATAN: _mouse_window TIDAK boleh di-null-kan di sini.
	// Mouse window adalah cursor fisik yang selalu ada dan tidak pernah hidden
	// secara normal. Jika di-null-kan, repaint() dan create_mouse_events()
	// akan crash (KRNL_READ_NONPAGED_AREA) karena keduanya tidak punya null guard.
	if(_focused_window && _focused_window->hidden())
		_focused_window = nullptr;
	if(_drag_window && _drag_window->hidden())
		_drag_window = nullptr;
	if(_resize_window && _resize_window->hidden())
		_resize_window = nullptr;
	if(_mousedown_window && _mousedown_window->hidden())
		_mousedown_window = nullptr;
	if(_prev_mouse_window && _prev_mouse_window->hidden())
		_prev_mouse_window = nullptr;
}

Display& Display::inst() {
	return *_inst;
}

ResizeMode Display::get_resize_mode(Gfx::Rect window, Gfx::Point mouse) {
	const int B = WINDOW_RESIZE_BORDER;
	const int C = WINDOW_RESIZE_BORDER * 2; // area corner lebih besar agar mudah dihit

	// Cek corner terlebih dahulu
	if(mouse.in({window.x, window.y, C, C}))
		return NORTHWEST;
	if(mouse.in({window.x + window.width - C, window.y, C, C}))
		return NORTHEAST;
	if(mouse.in({window.x, window.y + window.height - C, C, C}))
		return SOUTHWEST;
	if(mouse.in({window.x + window.width - C, window.y + window.height - C, C, C}))
		return SOUTHEAST;

	// Cek sisi setelah corner
	if(mouse.x < window.x + B)
		return WEST;
	if(mouse.x > window.x + window.width - B)
		return EAST;
	if(mouse.y < window.y + B)
		return NORTH;
	if(mouse.y > window.y + window.height - B)
		return SOUTH;

	return NONE;
}

Gfx::Rect Display::calculate_resize_rect() {
	if(!_resize_window)
		return {0, 0, 0, 0};

	Gfx::Point new_pos = _resize_window->rect().position();
	Gfx::Dimensions new_dims = _resize_window->rect().dimensions();
	Gfx::Point mouse_delta = _mouse_window->rect().position() - _resize_begin_point;
	switch(_resize_mode) {
		case NORTH:
			new_pos.y += mouse_delta.y;
			new_dims.height -= mouse_delta.y;
			break;
		case SOUTH:
			new_dims.height += mouse_delta.y;
			break;
		case WEST:
			new_pos.x += mouse_delta.x;
			new_dims.width -= mouse_delta.x;
			break;
		case EAST:
			new_dims.width += mouse_delta.x;
			break;
		case NORTHWEST:
			new_pos = new_pos + mouse_delta;
			new_dims.width -= mouse_delta.x;
			new_dims.height -= mouse_delta.y;
			break;
		case NORTHEAST:
			new_pos.y += mouse_delta.y;
			new_dims.width += mouse_delta.x;
			new_dims.height -= mouse_delta.y;
			break;
		case SOUTHWEST:
			new_pos.x += mouse_delta.x;
			new_dims.height += mouse_delta.y;
			new_dims.width -= mouse_delta.x;
			break;
		case SOUTHEAST:
			new_dims.height += mouse_delta.y;
			new_dims.width += mouse_delta.x;
			break;
		case NONE:
			break;
	}
	new_dims = {
		std::max(new_dims.width, _resize_window->minimum_size().width),
		std::max(new_dims.height, _resize_window->minimum_size().height)
	};
	return {new_pos.x, new_pos.y, new_dims.width, new_dims.height};
}