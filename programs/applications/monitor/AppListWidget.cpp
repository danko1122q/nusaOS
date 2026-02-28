/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2026 nusaOS */

#include "AppListWidget.h"
#include <dirent.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>

// Baca nama proses dari /proc/<pid>/cmdline — basename dari path pertama
static std::string read_proc_name(int pid) {
	char path[64];
	snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
	FILE* f = fopen(path, "r");
	if (!f) return "";
	char buf[256] = {};
	fread(buf, 1, sizeof(buf) - 1, f);
	fclose(f);
	if (buf[0] == '\0') return "";
	// cmdline null-separated, ambil token pertama lalu basename
	const char* slash = strrchr(buf, '/');
	return slash ? (slash + 1) : buf;
}

// Ambil list PID aktif dari /proc
static std::vector<int> get_pids() {
	std::vector<int> pids;
	DIR* dir = opendir("/proc");
	if (!dir) return pids;
	struct dirent* ent;
	while ((ent = readdir(dir)) != nullptr) {
		bool all_digit = true;
		for (int i = 0; ent->d_name[i]; i++) {
			if (ent->d_name[i] < '0' || ent->d_name[i] > '9') { all_digit = false; break; }
		}
		if (all_digit && ent->d_name[0] != '\0')
			pids.push_back(atoi(ent->d_name));
	}
	closedir(dir);
	return pids;
}

void AppListWidget::update() {
	m_apps.clear();

	// App::get_all_apps() return vector<App::Info>
	// App::Info::name() → display name
	// App::Info::hidden() → apakah hidden
	// App::Info di AppMenu juga punya run() tapi kita cuma perlu name()
	auto known_apps = App::get_all_apps();

	// Buat map: lowercase name → App::Info untuk matching dengan cmdline
	// Karena App::Info tidak punya executable(), kita cocokkan berdasarkan name
	// yang di-lowercase dan dibandingkan dengan basename cmdline
	std::map<std::string, std::string> name_map; // basename_lower → display_name
	for (auto& app : known_apps) {
		if (app.hidden() || app.name().empty()) continue;
		// Gunakan name() langsung sebagai key (lowercase)
		std::string key = app.name();
		for (auto& c : key) c = tolower(c);
		name_map[key] = app.name();
	}

	// Scan /proc untuk cari PID yang running
	auto pids = get_pids();
	std::set<std::string> seen; // dedup per display name

	for (int pid : pids) {
		std::string proc_name = read_proc_name(pid);
		if (proc_name.empty()) continue;

		// Lowercase proc_name untuk matching
		std::string proc_lower = proc_name;
		for (auto& c : proc_lower) c = tolower(c);

		// Cari exact match atau prefix match (misal "terminal" cocok "Terminal")
		std::string display_name;
		auto it = name_map.find(proc_lower);
		if (it != name_map.end()) {
			display_name = it->second;
		} else {
			// Coba cari yang namanya contain proc_name
			for (auto& [key, val] : name_map) {
				if (key.find(proc_lower) != std::string::npos ||
				    proc_lower.find(key) != std::string::npos) {
					display_name = val;
					break;
				}
			}
		}

		if (display_name.empty()) continue;
		if (seen.count(display_name)) continue;
		seen.insert(display_name);

		RunningApp ra;
		ra.name = display_name;
		ra.pid  = pid;
		m_apps.push_back(ra);
	}

	repaint();
}

void AppListWidget::do_repaint(const UI::DrawContext& ctx) {
	ctx.fill(ctx.rect(), UI::Theme::bg());

	if (m_apps.empty()) {
		ctx.draw_text("No apps", ctx.rect(),
			UI::CENTER, UI::CENTER,
			UI::Theme::font(), UI::Theme::fg());
		return;
	}

	// Gunakan size_of untuk hitung tinggi baris — pakai string pendek sebagai probe
	int font_h = UI::Theme::font()->size_of("M").height + 4;
	int y = 4;

	for (auto& app : m_apps) {
		if (y + font_h > ctx.height() - 4) break;

		// Bullet dot kecil warna accent
		ctx.fill({6, y + font_h / 2 - 2, 4, 4}, UI::Theme::accent());

		// Nama app
		ctx.draw_text(app.name.c_str(),
			{14, y, ctx.width() - 18, font_h},
			UI::BEGINNING, UI::CENTER,
			UI::Theme::font(), UI::Theme::fg());

		y += font_h + 1;
	}
}

Gfx::Dimensions AppListWidget::preferred_size() {
	return {160, 80};
}