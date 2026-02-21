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

#include "Mouse.h"
#include "Display.h"
#include "Server.h"
#include "Window.h"
#include "FontManager.h"
#include <libnusa/Log.h>
#include <string.h>
#include <poll.h>
#include <unistd.h>
#include <sys/wait.h>
#include <csignal>
#include <time.h>

// Path sandbar
static const char* SANDBAR_PATH = "/apps/sandbar.app/sandbar";

// PID sandbar yang sedang berjalan
static pid_t sandbar_pid = -1;

// Timestamp terakhir sandbar di-launch (untuk mencegah restart loop terlalu cepat)
static time_t sandbar_last_launch = 0;
#define SANDBAR_RESTART_DELAY_SEC 2

static void launch_sandbar() {
	time_t now = time(nullptr);

	// FIX: jangan restart kalau baru saja crash (< 2 detik yang lalu)
	// ini mencegah restart loop yang bisa membekukan pond
	if(now - sandbar_last_launch < SANDBAR_RESTART_DELAY_SEC) {
		Duck::Log::warn("Sandbar crashed too quickly, delaying restart...");
		return;
	}
	sandbar_last_launch = now;

	pid_t pid = fork();
	if(pid == 0) {
		// Child process — jalankan sandbar
		// FIX: argv asli kosong total (NULL langsung) padahal argv[0] harus ada
		// beberapa libc mengharapkan argv[0] = nama program
		char* args[] = { (char*) SANDBAR_PATH, nullptr };
		char* envp[] = { nullptr };
		execve(SANDBAR_PATH, args, envp);
		// Kalau execve gagal
		Duck::Log::errf("Failed to launch sandbar: {}", strerror(errno));
		exit(-1);
	} else if(pid > 0) {
		sandbar_pid = pid;
		Duck::Log::successf("Sandbar launched with PID {}", pid);
	} else {
		Duck::Log::err("Failed to fork for sandbar");
	}
}

// FIX: SIGCHLD handler yang proper — cek apakah yang mati adalah sandbar,
// kalau iya restart dia. Aslinya hanya wait() tanpa restart sama sekali.
static void sigchld_handler(int) {
	int status;
	pid_t pid;

	// Pakai WNOHANG supaya tidak block event loop pond
	while((pid = waitpid(-1, &status, WNOHANG)) > 0) {
		if(pid == sandbar_pid) {
			Duck::Log::warn("Sandbar (PID ", pid, ") died, restarting...");
			sandbar_pid = -1;
			launch_sandbar();
		}
	}
}

int main(int argc, char** argv, char** envp) {
	// Setup SIGCHLD handler dulu sebelum fork
	// FIX: SA_RESTART belum diimplementasikan di nusaOS, pakai signal() biasa
	signal(SIGCHLD, sigchld_handler);

	auto* display = new Display;
	auto* server = new Server;
	auto* main_window = new Window(display);
	main_window->set_hidden(false);
	auto* mouse = new Mouse(main_window);
	auto* font_manager = new FontManager();

	struct pollfd polls[3];
	polls[0].fd = mouse->fd();
	polls[0].events = POLLIN;
	polls[1].fd = server->fd();
	polls[1].events = POLLIN;
	polls[2].fd = display->keyboard_fd();
	polls[2].events = POLLIN;

	// Launch sandbar dengan mekanisme restart yang proper
	launch_sandbar();

	Duck::Log::success("Pond started!");

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
	while(true) {
		poll(polls, 3, display->buffer_is_dirty() ? display->millis_until_next_flip() : -1);
		mouse->update();
		display->update_keyboard();
		server->handle_packets();
		display->repaint();
	}
#pragma clang diagnostic pop
}