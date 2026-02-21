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
    
    Copyright (c) Byteduck 2016-2020. All rights reserved.
*/

// A program that lists the contents of a directory.

#include <libnusa/Args.h>
#include <libnusa/Path.h>
#include <libnusa/FormatStream.h>
#include <unistd.h>
#include <algorithm>
#include <termios.h>
#include <sys/ioctl.h>

bool g_colorize = false;
bool g_no_color = false;
bool g_human = false;
bool g_show_all = false;
bool g_long_format = false;
bool g_columns = false;

constexpr auto RESET_FORMAT = "\033[39m";

constexpr char ENTRY_TYPE_CHARS[] = {
		'?', // DT_UNKNOWN
		'-', // DT_REG
		'd', // DT_DIR
		'c', // DT_CHR
		'b', // DT_BLK
		'f', // DT_FIFO
		's', // DT_SOCK
		'l', // DT_LNK
};

constexpr const char* ENTRY_TYPE_COLORS[] = {
		"",         // DT_UNKNOWN
		"\033[39m", // DT_REG
		"\033[36m", // DT_DIR
		"\033[32m", // DT_CHR
		"\033[33m", // DT_BLK
		"\033[35m", // DT_FIFO
		"\033[38m", // DT_SOCK
		"\033[34m", // DT_LNK
};

std::string entry_permissions_string(const Duck::DirectoryEntry& entry) {
	constexpr char bit_names[] = {'r', 'w', 'x'};
	constexpr const char* bit_colors[] {"\033[36m", "\033[31m", "\033[32m"};
	Duck::StringOutputStream stream;
	for(mode_t bit = 0; bit < 9; bit++) {
		if(entry.mode() & (0x1u << (8 - bit))) {
			if(g_colorize)
				stream << bit_colors[bit % 3] << bit_names[bit % 3] << RESET_FORMAT;
			else
				stream << bit_names[bit % 3];
		} else {
			stream << '-';
		}
	}
	return stream.string();
}

std::string entry_name(const Duck::DirectoryEntry& entry) {
	Duck::StringOutputStream out;
	if(!g_no_color)
		out << ENTRY_TYPE_COLORS[entry.type()] << entry.name() << RESET_FORMAT;
	else
		out << entry.name();
	return out.string();
}

std::string entry_size_str(const Duck::DirectoryEntry& entry, size_t widest_size) {
	std::string size_str = g_human ? entry.size().readable() : std::to_string(entry.size().bytes);
	if(widest_size)
		return std::string(widest_size - size_str.size(), ' ') + size_str;
	return size_str;
};

int main(int argc, char** argv) {
	std::string dir_name = ".";

	Duck::Args args;
	args.add_positional(dir_name, false, "DIR", "The directory to list.");
	args.add_named(g_no_color, "n", "no-color", "Do not colorize the output.");
	args.add_named(g_colorize, "C", "color", "Colorize the output.");   // FIX: ubah -c jadi -C supaya tidak bentrok
	args.add_named(g_show_all, "a", "all", "Show entries starting with \".\".");
	args.add_named(g_long_format, "l", "long", "Show more details for entries.");
	args.add_named(g_human, "h", "human-readable", "Show human-readable sizes.");
	args.add_named(g_columns, "c", "columns", "Force multi-column output.");
	args.parse(argc, argv);

	if(!isatty(STDOUT_FILENO))
		g_no_color = true;
	else
		g_columns = true;

	g_columns = g_columns && !g_long_format;

	// Read entries
	auto dirs_res = Duck::Path(dir_name).get_directory_entries();
	if(dirs_res.is_error()) {
		Duck::printerrln("ls: cannot access {}: {}", dir_name, dirs_res.strerror());
		return dirs_res.code();
	}

	// Sort by name, lalu by type (stable agar urutan nama tetap terjaga per-tipe)
	auto entries = dirs_res.value();
	std::sort(entries.begin(), entries.end(), [](auto const& lhs, auto const& rhs) {
		// FIX: sort stabil: utamakan tipe (dir dulu), lalu nama
		if(lhs.type() != rhs.type())
			return lhs.type() > rhs.type();
		return lhs.name() < rhs.name();
	});

	// Helper lambda: apakah entry ini harus ditampilkan?
	auto should_show = [&](const Duck::DirectoryEntry& entry) -> bool {
		return g_show_all || entry.name()[0] != '.';
	};

	if(g_long_format) {
		// Hitung lebar kolom size terlebih dahulu
		size_t widest_size = 0;
		for(auto& entry : entries) {
			if(!should_show(entry)) continue;
			widest_size = std::max(widest_size, entry_size_str(entry, 0).size());
		}

		// Cetak setiap entry
		for(auto& entry : entries) {
			if(!should_show(entry)) continue;
			Duck::Stream::std_out
					<< ENTRY_TYPE_CHARS[entry.type()] << entry_permissions_string(entry) << ' '
					<< entry_size_str(entry, widest_size) << ' '
					<< entry_name(entry) << '\n';
		}
	} else if(g_columns) {
		// Dapatkan lebar terminal
		winsize winsz;
		winsz.ws_col = 80;
		ioctl(STDOUT_FILENO, TIOCGWINSZ, &winsz);

		// FIX: Kumpulkan dulu entry yang akan ditampilkan, baru hitung kolom
		std::vector<Duck::DirectoryEntry> visible_entries;
		for(auto& entry : entries) {
			if(should_show(entry))
				visible_entries.push_back(entry);
		}

		size_t widest_name = 0;
		for(auto& entry : visible_entries) {
			widest_name = std::max(widest_name, entry.name().size());
		}

		constexpr size_t padding = 2;
		auto n_cols = std::max((int) winsz.ws_col / (int) (widest_name + padding), 1);
		auto n_rows = ((int)visible_entries.size() + n_cols - 1) / n_cols;

		// FIX: Gunakan visible_entries (bukan entries yang masih ada hidden files)
		std::vector<std::vector<Duck::DirectoryEntry>> cols;
		cols.emplace_back();
		for (auto& entry : visible_entries) {
			if ((int)cols[cols.size() - 1].size() == n_rows)
				cols.emplace_back();
			cols[cols.size() - 1].push_back(entry);
		}

		for (auto row = 0; row < n_rows; row++) {
			for(auto col = 0; col < n_cols; col++) {
				auto& col_entries = cols[col];
				if ((int)col_entries.size() <= row)
					continue;
				Duck::Stream::std_out << entry_name(col_entries[row]);
				if (col != n_cols - 1) {
					auto pad = padding + widest_name - col_entries[row].name().size();
					for (auto i = 0; i < pad; i++)
						Duck::Stream::std_out << ' ';
				}
			}
			Duck::Stream::std_out << '\n';
		}
	} else {
		// Short format
		for(auto& entry : entries) {
			if(!should_show(entry)) continue;
			Duck::Stream::std_out << entry_name(entry) << '\n';
		}
	}

	return 0;
}