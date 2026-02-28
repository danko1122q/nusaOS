/*
    This file is part of nusaOS.
    Copyright (c) Byteduck 2016-2022. All rights reserved.
*/

#include "MemoryUsageWidget.h"
#include <libgraphics/Font.h>

using namespace UI;
using namespace Sys;

void MemoryUsageWidget::update(Mem::Info info) {
	m_mem_info = info;
	repaint();
}

Gfx::Dimensions MemoryUsageWidget::preferred_size() {
	auto mem_text = "Memory: " + m_mem_info.used.readable() + " / " + m_mem_info.usable.readable();
	return {Theme::font()->size_of(mem_text.c_str()).width + 20, Theme::progress_bar_height()};
}

void MemoryUsageWidget::do_repaint(const DrawContext& ctx) {
	ctx.draw_inset_rect({0, 0, ctx.width(), ctx.height()});
	Gfx::Rect bar_area = {2, 2, ctx.width() - 4, ctx.height() - 3};

	// GUARD: usable == 0 → data belum dimuat (render sebelum timer pertama fire).
	// Tanpa guard: (double)x / 0.0 = NaN → (int)(NaN × width) = INT_MIN/undefined.
	if (!m_mem_info.usable) {
		ctx.draw_text("Memory: loading...", bar_area, CENTER, CENTER, Theme::font(), Theme::fg());
		return;
	}

	// GUARD: Mem::Amount adalah UNSIGNED. Jika terjadi kondisi edge case di mana
	// used < kernel_phys atau kernel_phys < kernel_disk_cache (bisa terjadi saat
	// disk cache baru di-flush setelah viewer/file manager ditutup dan /proc/meminfo
	// dibaca di momen tengah-tengah kernel update counter), maka pengurangan unsigned
	// akan UNDERFLOW → nilai ~4GB → int overflow → crash di graphics fill.
	// Solusi: gunakan signed arithmetic dengan clamping ke [0, bar_area.width].

	auto safe_sub = [](auto a, auto b) -> double {
		return (a > b) ? (double)(a - b) : 0.0;
	};

	double usable = (double) m_mem_info.usable;

	// Hitung pixel width masing-masing segment, clamp ke [0, bar_area.width]
	int kernel = (int)((safe_sub(m_mem_info.kernel_phys, m_mem_info.kernel_disk_cache) / usable) * bar_area.width);
	int disk   = (int)((safe_sub(m_mem_info.kernel_disk_cache, 0)                       / usable) * bar_area.width);
	int user   = (int)((safe_sub(m_mem_info.used, m_mem_info.kernel_phys)               / usable) * bar_area.width);

	// Clamp individual segments ke non-negatif
	kernel = std::max(kernel, 0);
	disk   = std::max(disk,   0);
	user   = std::max(user,   0);

	// Pastikan total tidak melebihi lebar bar — cegah draw di luar framebuffer
	int total = kernel + disk + user;
	if (total > bar_area.width) {
		double scale = (double) bar_area.width / total;
		kernel = (int)(kernel * scale);
		disk   = (int)(disk   * scale);
		user   = (int)(user   * scale);
	}

	if (kernel > 0)
		ctx.draw_outset_rect({bar_area.x, bar_area.y, kernel, bar_area.height}, UI::Theme::accent());

	if (disk > 0)
		ctx.draw_outset_rect({bar_area.x + kernel, bar_area.y, disk, bar_area.height}, RGB(219, 112, 147));

	if (user > 0)
		ctx.draw_outset_rect({bar_area.x + kernel + disk, bar_area.y, user, bar_area.height}, RGB(46,139,87));

	auto mem_text = "Memory: " + m_mem_info.used.readable() + " / " + m_mem_info.usable.readable();
	ctx.draw_text(mem_text.c_str(), bar_area, CENTER, CENTER, Theme::font(), Theme::fg());
}

MemoryUsageWidget::MemoryUsageWidget(): m_mem_info({}) {}