/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 danko1122q */

#include <libui/libui.h>
#include <libapp/App.h>
#include "HexEditor.h"

int main(int argc, char** argv, char** envp) {
	UI::init(argv, envp);

	auto editor = HexEditor::make();

	// If a file path was passed as argument, open it immediately
	if(argc >= 2)
		editor->open_file(argv[1]);

	UI::run();
	return 0;
}