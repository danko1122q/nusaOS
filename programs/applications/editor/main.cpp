/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2022 Byteduck */
#include <libui/libui.h>
#include <libui/widget/Label.h>
#include <libui/bits/FilePicker.h>
#include <libui/widget/TextView.h>
#include <libui/widget/MenuBar.h>

using namespace Duck;
using namespace UI;
using namespace Keyboard;

int main(int argc, char** argv, char** envp) {
	UI::init(argv, envp);

	Duck::Path file_path = "/";

	auto window = UI::Window::make();
	auto text_view = UI::TextView::make("");

	std::function<void()> save_as;

	// FIX: save tidak pakai TRY karena lambda-nya return void.
	// Gunakan is_error() check manual agar tidak ada return type mismatch.
	auto save = [&] () {
		if (file_path.is_dir()) {
			save_as();
			return;
		}
		auto file_res = File::open(file_path, "w+");
		if (file_res.is_error())
			return;
		auto& file = file_res.value();
		file.write(text_view->text().data(), text_view->text().length());
		file.close();
	};

	save_as = [&] () {
		auto filename = FilePicker::make(FilePicker::Mode::SAVE, "/untitled.txt")->pick();
		if (filename.empty())
			return;
		file_path = filename[0];
		window->set_title("Editor: " + file_path.basename());
		save();
	};

	// FIX: open() tidak lagi menggunakan TRY() di dalam lambda.
	// TRY macro melakukan `return` dari scope saat ini — di dalam lambda yang
	// return void, ini adalah undefined behavior dan menyebabkan BSOD karena
	// stack frame rusak saat TRY() mencoba return Result dari void lambda.
	// Ganti semua TRY dengan is_error() check eksplisit.
	auto open = [&] (Duck::Path path) {
		file_path = path;
		window->set_title("Editor: " + path.basename());
		window->set_icon(UI::app_info().icon_for_file(file_path));

		auto file_res = File::open(path, "r");
		if (file_res.is_error())
			return;
		auto& file = file_res.value();

		std::string contents;
		FileInputStream stream(file);
		while (true) {
			char read;
			if (stream.read(&read, 1) != 1 || contents.size() > (1024 * 1024))
				break;
			if (read)
				contents += read;
		}
		file.close();

		text_view->set_text(std::move(contents));
	};

	auto open_picker = [&] () {
		auto files = UI::FilePicker::make()->pick();
		if (files.empty())
			return;
		open(files[0]);
	};

	auto menu = UI::Menu::make({
		MenuItem::make("File", std::vector {
			MenuItem::make("Open...", open_picker, Shortcut {Key::O, Modifier::Ctrl}),
			MenuItem::make("Save", save, Shortcut {Key::S, Modifier::Ctrl}),
			MenuItem::make("Save As...", save_as, Shortcut {Key::S, Modifier::Ctrl | Modifier::Shift})
		}),
		MenuItem::make("View", std::vector {
			MenuItem::make("Line Breaks", std::vector {
				MenuItem::make("Word", [&]() { text_view->set_break_mode(UI::TextLayout::BreakMode::WORD); }),
				MenuItem::make("Character", [&]() { text_view->set_break_mode(UI::TextLayout::BreakMode::CHARACTER); })
			})
		})
	});

	auto menu_bar = UI::MenuBar::make(menu);
	window->set_titlebar_accessory(menu_bar);

	window->set_contents(text_view);
	window->set_resizable(true);
	window->set_title("Editor");

	if (argc >= 2)
		open(argv[1]);

	auto disp_rect = Gfx::Rect {{0,0}, UI::pond_context->get_display_dimensions() - Gfx::Dimensions {32, 32}};
	if (!Gfx::Rect {{0, 0}, window->dimensions()}.inside(disp_rect))
		window->resize(UI::pond_context->get_display_dimensions() - Gfx::Dimensions {32, 32});

	window->show();
	UI::run();
}