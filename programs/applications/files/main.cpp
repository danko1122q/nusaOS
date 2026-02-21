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

#include <libui/libui.h>
#include <libui/widget/files/FileGridView.h>
#include <libui/widget/layout/FlexLayout.h>
#include <libui/widget/files/FileNavigationBar.h>

using namespace Duck;

class FileManager: public Duck::Object, public UI::FileViewDelegate {
public:
	NUSA_OBJECT_DEF(FileManager);

	void fv_did_select_files(std::vector<Duck::Path> selected) override {
		(void) selected;
	}

	void fv_did_double_click(Duck::DirectoryEntry entry) override {
		if (entry.path().string().empty())
			return;

		// Null check sebelum akses
		if (!file_grid)
			return;

		if (entry.is_directory()) {
			file_grid->set_directory(entry.path());
		} else {
			App::open(entry.path());
		}
	}

	void fv_did_navigate(Duck::Path path) override {
		// Guard terhadap null pointer
		if (!header || path.string().empty())
			return;
		header->fv_did_navigate(path);
	}

protected:
	void initialize() override {
		// Inisialisasi file_grid dulu
		file_grid = UI::FileGridView::make("/");
		if (!file_grid)
			return;
		
		// Baru buat header SETELAH file_grid valid
		header = UI::FileNavigationBar::make(file_grid);
		if (!header)
			return;

		auto main_flex = UI::FlexLayout::make(UI::FlexLayout::VERTICAL);
		if (!main_flex)
			return;

		file_grid->delegate = self();
		main_flex->add_child(file_grid);

		auto window = UI::Window::make();
		if (!window)
			return;

		window->set_titlebar_accessory(header);
		window->set_contents(main_flex);
		window->set_resizable(true);
		window->set_title("Files");
		window->resize({306, 300});
		window->show();
	}

private:
	// Jangan initialize di sini - pindah ke initialize()
	Ptr<UI::FileGridView>      file_grid;
	Ptr<UI::FileNavigationBar> header;
};

int main(int argc, char** argv, char** envp) {
	if (!argv || !envp)
		return 1;

	UI::init(argv, envp);
	auto fm = FileManager::make();
	if (!fm)
		return 1;

	UI::run();
	return 0;
}