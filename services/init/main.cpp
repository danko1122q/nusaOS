/*
    This file is part of nusaOS.
    Copyright (c) Byteduck 2016-2020. All rights reserved.
*/

#include <csignal>
#include <cstdio>
#include <unistd.h>
#include <sys/wait.h>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <libnusa/Log.h>
#include "Service.h"

using Duck::Log;

// FIX Bug 1: SIGCHLD handler — reap zombie processes dengan WNOHANG
// agar tidak blocking dan tidak accumulate zombie
static void sigchld_handler(int) {
	int status;
	while(waitpid(-1, &status, WNOHANG) > 0);
}

int main(int argc, char** argv, char** envp) {
	if(getpid() != 1) {
		printf("pid != 1. Exiting.\n");
		return -1;
	}

	setsid();
	signal(SIGCHLD, sigchld_handler);

	Log::success("Welcome to nusaOS!");

	auto services = Service::get_all_services();

	// Jalankan semua service "boot"
	// FIX Bug 2 & 4: Pass envp dari main, jalankan berurutan dengan jeda kecil
	// Note: Sandbar dilauncher oleh Pond sendiri, bukan dari sini —
	// jadi tidak perlu logic after=pond di init
	for(auto& service : services) {
		if(service.after() != "boot")
			continue;
		service.execute(envp);
		// Jeda kecil antar service agar tidak semua start bersamaan
		// dan menyebabkan resource contention saat boot
		usleep(50 * 1000); // 50ms
	}

	// Tunggu semua child process
	while(true) {
		int status;
		pid_t pid = waitpid(-1, &status, 0);
		if(pid < 0) {
			if(errno == ECHILD) break;
			if(errno == EINTR) continue;
			break;
		}
		Log::info("Service pid ", pid, " exited with status ", WEXITSTATUS(status));
	}

	Log::info("All services exited. Goodbye!");
	return 0;
}