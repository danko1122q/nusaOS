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

static const char* SANDBAR_PATH = "/apps/sandbar.app/sandbar";
static const char* DESKTOP_PATH = "/apps/desktop.app/desktop";

static pid_t sandbar_pid = -1;
static pid_t desktop_pid = -1;

static time_t sandbar_last_launch = 0;
static time_t desktop_last_launch = 0;
#define RESTART_DELAY_SEC 2

static pid_t launch_app(const char* path, time_t& last_launch, const char* name) {
	time_t now = time(nullptr);
	if(now - last_launch < RESTART_DELAY_SEC) {
		Duck::Log::warnf("{} crashed too quickly, delaying restart...", name);
		return -1;
	}
	last_launch = now;

	pid_t pid = fork();
	if(pid == 0) {
		char* args[] = { (char*) path, nullptr };
		char* envp[] = { nullptr };
		execve(path, args, envp);
		Duck::Log::errf("Failed to launch {}: {}", name, strerror(errno));
		exit(-1);
	} else if(pid > 0) {
		Duck::Log::successf("{} launched with PID {}", name, pid);
	} else {
		Duck::Log::errf("Failed to fork for {}", name);
	}
	return pid;
}

static void sigchld_handler(int) {
	int status;
	pid_t pid;
	while((pid = waitpid(-1, &status, WNOHANG)) > 0) {
		if(pid == sandbar_pid) {
			Duck::Log::warn("Sandbar died, restarting...");
			sandbar_pid = -1;
			sandbar_pid = launch_app(SANDBAR_PATH, sandbar_last_launch, "Sandbar");
		} else if(pid == desktop_pid) {
			Duck::Log::warn("Desktop died, restarting...");
			desktop_pid = -1;
			desktop_pid = launch_app(DESKTOP_PATH, desktop_last_launch, "Desktop");
		}
	}
}

int main(int argc, char** argv, char** envp) {
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

	// Launch desktop (DESKTOP layer) dahulu, baru sandbar (PANEL layer)
	desktop_pid = launch_app(DESKTOP_PATH, desktop_last_launch, "Desktop");
	sandbar_pid = launch_app(SANDBAR_PATH, sandbar_last_launch, "Sandbar");

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