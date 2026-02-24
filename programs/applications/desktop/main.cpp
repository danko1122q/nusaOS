/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2025-2026 danko1122q */

#include <libui/libui.h>
#include <csignal>
#include <sys/wait.h>
#include "Desktop.h"

void sigchld_handler(int) {
	int dummy;
	wait(&dummy);
}

int main(int argc, char** argv, char** envp) {
	signal(SIGCHLD, sigchld_handler);
	UI::init(argv, envp);
	auto desktop = Desktop::make();
	UI::run();
	return 0;
}