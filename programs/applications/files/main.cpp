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
		// Guard: pastikan semua pointer valid dan path tidak kosong
		if (!file_grid || entry.path().string().empty())
			return;

		if (entry.is_directory()) {
			file_grid->set_directory(entry.path());
		} else {
			App::open(entry.path());
		}
	}

	void fv_did_navigate(Duck::Path path) override {
		// FIX: Guard ganda - header DAN file_grid harus valid dulu
		// fv_did_navigate bisa dipanggil sangat awal (saat set_directory di initialize)
		// sebelum header selesai dibuat. Dengan flag m_initialized kita skip dulu.
		if (!m_initialized || !header || path.string().empty())
			return;
		header->fv_did_navigate(path);
	}

protected:
	void initialize() override {
		// FIX: Urutan kritis - delegate JANGAN di-set dulu sebelum semua siap.
		// 1. Buat file_grid tanpa delegate dulu, supaya callback tidak terpanggil
		//    ke object yang belum fully initialized.
		file_grid = UI::FileGridView::make("/");
		if (!file_grid)
			return;

		// 2. Buat header setelah file_grid valid
		header = UI::FileNavigationBar::make(file_grid);
		if (!header)
			return;

		// 3. Buat layout
		auto main_flex = UI::FlexLayout::make(UI::FlexLayout::VERTICAL);
		if (!main_flex)
			return;
		main_flex->add_child(file_grid);

		// 4. Buat window
		auto window = UI::Window::make();
		if (!window)
			return;
		window->set_titlebar_accessory(header);
		window->set_contents(main_flex);
		window->set_resizable(true);
		window->set_title("Files");
		window->resize({306, 300});

		// 5. FIX: Set flag SEBELUM show() supaya kalau show() trigger navigate,
		//    callback sudah bisa jalan dengan aman.
		m_initialized = true;

		// 6. FIX: Baru set delegate SETELAH semua widget siap dan flag aktif.
		//    Ini mencegah Arc::m_ptr assertion karena self() dipanggil
		//    sebelum make() selesai meng-assign pointer.
		file_grid->delegate = self();

		window->show();
	}

private:
	Ptr<UI::FileGridView>      file_grid;
	Ptr<UI::FileNavigationBar> header;

	// FIX: Flag untuk melindungi callback yang masuk sebelum inisialisasi selesai
	bool m_initialized = false;
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