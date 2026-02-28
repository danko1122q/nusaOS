/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2022 Byteduck */
#include <libui/libui.h>
#include <libui/widget/Label.h>
#include <libui/widget/Image.h>
#include <libui/bits/FilePicker.h>
#include <libui/widget/TextView.h>
#include "ViewerWidget.h"
#include "ViewerAudioWidget.h"
#include <libui/widget/Cell.h>

using namespace Duck;

// FIX: open_image dan open_audio tetap return Result — ini aman karena
// dipanggil dari main() yang bisa handle Result, BUKAN dari void lambda.
// Sebelumnya tidak ada bug di sini — biarkan seperti ini.

Result open_image(Duck::Ptr<UI::Window> window, Duck::Path file_path) {
	auto image = TRY(Gfx::Image::load(file_path));
	auto img = ViewerWidget::make(image);
	img->set_sizing_mode(UI::FILL);
	window->set_contents(img);
	window->set_resizable(true);
	return Result::SUCCESS;
}

Result open_audio(Duck::Ptr<UI::Window> window, Duck::Path file_path) {
	// FIX: Jangan pakai TRY() di sini jika File::open() mengembalikan
	// tipe yang bukan Result<T> yang kompatibel dengan return type fungsi ini.
	// Gunakan explicit check untuk konsistensi dan safety.
	auto file_res = File::open(file_path, "r");
	if (file_res.is_error())
		return file_res.result();
	auto& file = file_res.value();

	auto wav_res = Sound::WavReader::read_wav(file);
	if (wav_res.is_error())
		return wav_res.result();

	auto widget = ViewerAudioWidget::make(wav_res.value());
	window->set_contents(UI::Cell::make(widget));
	return Result::SUCCESS;
}

int main(int argc, char** argv, char** envp) {
	UI::init(argv, envp);

	Duck::Path file_path = "/";
	if (argc < 2) {
		auto files = UI::FilePicker::make()->pick();
		if (files.empty())
			return EXIT_SUCCESS;
		file_path = files[0];
	} else {
		file_path = argv[1];
	}

	auto window = UI::Window::make();
	Result result = Result("Unsupported filetype");

	if (file_path.extension() == "png" || file_path.extension() == "icon") {
		result = open_image(window, file_path);
	} else if (file_path.extension() == "wav") {
		result = open_audio(window, file_path);
	}

	if (result.is_error()) {
		auto error_label = UI::Label::make(result.has_message() ? result.message() : "Error opening file");
		window->set_contents(UI::Cell::make(error_label));
		window->set_title("Viewer");
		window->resize({350, 100});
	} else {
		window->set_title("Viewer: " + file_path.basename());
		// icon_for_file() bisa return nullptr jika extension tidak dikenal
		// atau file icon tidak ada di filesystem.
		// Window::set_icon(nullptr) → null dereference → BSOD langsung saat viewer buka.
		auto file_icon = UI::app_info().icon_for_file(file_path);
		if (file_icon)
			window->set_icon(file_icon);
	}

	auto disp_rect = Gfx::Rect {{0,0}, UI::pond_context->get_display_dimensions() - Gfx::Dimensions {32, 32}};
	if (!Gfx::Rect {{0, 0}, window->dimensions()}.inside(disp_rect))
		window->resize(UI::pond_context->get_display_dimensions() - Gfx::Dimensions {32, 32});

	window->show();
	UI::run();
}