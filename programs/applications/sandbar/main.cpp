/*
	This file is part of nusaOS.
	Copyright (c) Byteduck 2016-2021. All rights reserved.
*/

#include <libui/libui.h>
#include <libui/widget/Button.h>
#include <libui/widget/layout/BoxLayout.h>
#include <libapp/App.h>
#include <csignal>
#include <sys/wait.h>
#include "libgraphics/PNG.h"
#include "libui/widget/Image.h"
#include "Sandbar.h"

void sigchld_handler(int sig) {
	int dummy;
	wait(&dummy);
}

int main(int argc, char** argv, char** envp) {
	signal(SIGCHLD, sigchld_handler);

	UI::init(argv, envp);
	auto sandbar = Sandbar::make();
	UI::run();

	return 0;
}