/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2022 Byteduck */

#include "FileGridView.h"
#include "../Button.h"
#include "../Image.h"
#include "../../libui.h"
#include "../Cell.h"
#include "../layout/FlexLayout.h"
#include "../../Menu.h"
#include <libapp/App.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>

using namespace UI;

class FileView: public UI::FlexLayout {
public:
	WIDGET_DEF(FileView)
protected:
	bool on_mouse_button(Pond::MouseButtonEvent evt) override {
		// Klik kiri - select / double-click open
		if((evt.old_buttons & POND_MOUSE1) && !(evt.new_buttons & POND_MOUSE1)) {
			if(auto w = dir_widget.lock())
				w->clicked_entry(entry);
			return true;
		}
		// Klik kanan - context menu
		if(!(evt.old_buttons & POND_MOUSE2) && (evt.new_buttons & POND_MOUSE2)) {
			show_context_menu();
			return true;
		}
		return false;
	}

private:
	FileView(const Duck::DirectoryEntry& dir_entry, Duck::WeakPtr<FileGridView> dir_widget):
		FlexLayout(VERTICAL), entry(dir_entry), dir_widget(std::move(dir_widget))
	{
		Duck::Ptr<const Gfx::Image> image;
		auto path = entry.path();

		if(path.extension() == "icon" || path.extension() == "png")
			image = Gfx::Image::load(entry.path()).value_or(nullptr);

		if(!image) {
			auto app = App::app_for_file(path);
			if(app.has_value())
				image = app.value().icon_for_file(path);
			else
				image = UI::icon(entry.is_directory() ? "/filetypes/folder" : "/filetypes/default");
		}

		auto ui_image = UI::Image::make(image);
		ui_image->set_preferred_size({32, 32});
		ui_image->set_sizing_mode(UI::PREFERRED);
		add_child(ui_image);

		auto label = UI::Label::make(entry.name());
		label->set_alignment(CENTER, CENTER);
		label->set_sizing_mode(UI::FILL);
		add_child(label);
	}

	Duck::DirectoryEntry entry;
	Duck::WeakPtr<FileGridView> dir_widget;

	void show_context_menu() {
		auto menu = UI::Menu::make();

		Duck::DirectoryEntry entry_copy = entry;
		Duck::WeakPtr<FileGridView> widget_copy = dir_widget;

		// "Open" - selalu ada untuk file maupun folder
		menu->add_item(UI::MenuItem::make("Open", [entry_copy, widget_copy]() mutable {
			if(auto w = widget_copy.lock())
				w->open_entry(entry_copy);
		}));

		menu->add_item(UI::MenuItem::Separator);

		// Label berbeda: "Delete Folder" vs "Delete File"
		if(entry.is_directory()) {
			menu->add_item(UI::MenuItem::make("Delete Folder", [entry_copy, widget_copy]() mutable {
				if(auto w = widget_copy.lock())
					w->delete_entry(entry_copy, true);
			}));
		} else {
			menu->add_item(UI::MenuItem::make("Delete File", [entry_copy, widget_copy]() mutable {
				if(auto w = widget_copy.lock())
					w->delete_entry(entry_copy, false);
			}));
		}

		open_menu(menu);
	}
};

FileGridView::FileGridView(const Duck::Path& path) {
	set_directory(path);
	set_sizing_mode(FILL);
}

void FileGridView::initialize() {
	list_view->delegate = self();
	list_view->set_sizing_mode(FILL);
	add_child(list_view);
	inited = true;
}

Duck::Ptr<Widget> FileGridView::lv_create_entry(int index) {
	auto entry = entries()[index];
	bool is_selected = std::find(m_selected.begin(), m_selected.end(), entry.path()) != m_selected.end();
	auto fv = FileView::make(entry, self());
	auto pee = Cell::make(fv, 4, is_selected ? RGBA(255, 255, 255, 50) : RGBA(0, 0, 0, 0));
	return pee;
}

Gfx::Dimensions FileGridView::lv_preferred_item_dimensions() {
	return { 70, 70 };
}

int FileGridView::lv_num_items() {
	return entries().size();
}

void FileGridView::did_set_directory(Duck::Path path) {
	if(inited) {
		m_selected.clear();
		if(!delegate.expired())
			delegate.lock()->fv_did_select_files(m_selected);
		list_view->update_data();
		list_view->scroll_to({0, 0});
	}
}

void FileGridView::clicked_entry(Duck::DirectoryEntry entry) {
	bool is_selected = std::find(m_selected.begin(), m_selected.end(), entry.path()) != m_selected.end();
	if(is_selected) {
		if(App::app_for_file(entry.path()).is_error()) {
			set_directory(entry.path());
		} else {
			if(!delegate.expired())
				delegate.lock()->fv_did_double_click(entry);
		}
	} else {
		m_selected.clear();
		m_selected.push_back(entry.path());
		list_view->update_data();
		if(!delegate.expired())
			delegate.lock()->fv_did_select_files(m_selected);
	}
}

void FileGridView::open_entry(Duck::DirectoryEntry entry) {
	if(entry.is_directory()) {
		set_directory(entry.path());
	} else {
		// Lewat delegate supaya FileManager bisa membuka editor dengan benar
		if(!delegate.expired())
			delegate.lock()->fv_did_double_click(entry);
		else
			App::open(entry.path()); // fallback jika tidak ada delegate
	}
}

Gfx::Dimensions FileGridView::minimum_size() {
	return { 92, 92 };
}

// Rekursif delete: hapus semua isi direktori lalu direktorinya sendiri
static void remove_recursive(const std::string& path) {
	DIR* dir = ::opendir(path.c_str());
	if(dir) {
		struct dirent* ent;
		while((ent = ::readdir(dir)) != nullptr) {
			if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
				continue;
			// Bangun path child pakai string biasa, hindari Duck::Path operator/
			std::string child = path;
			if(child.back() != '/')
				child += '/';
			child += ent->d_name;
			remove_recursive(child);
		}
		::closedir(dir);
		::rmdir(path.c_str());
	} else {
		::unlink(path.c_str());
	}
}

void FileGridView::delete_entry(Duck::DirectoryEntry entry, bool is_directory) {
	remove_recursive(entry.path().string());
	// Refresh view setelah hapus
	m_selected.clear();
	set_directory(current_directory());
}