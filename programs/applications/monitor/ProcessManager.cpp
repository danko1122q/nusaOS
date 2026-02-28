/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2023 Byteduck */

#include "ProcessManager.h"

ProcessManager* ProcessManager::s_inst = nullptr;

ProcessManager::ProcessManager() {}

ProcessManager& ProcessManager::inst() {
	if (!s_inst)
		s_inst = new ProcessManager();
	return *s_inst;
}

void ProcessManager::update() {
	// Sys::Process::get_all() membaca /proc. Proses bisa exit di tengah pembacaan
	// sehingga beberapa entry /proc/[pid] mungkin hilang. Jika get_all() gagal
	// atau throw, jaga snapshot lama agar UI tidak crash dengan data kosong.
	auto result = Sys::Process::get_all();
	if(!result.empty())
		m_processes = result;
	// Jika result kosong (semua proses hilang — tidak mungkin di kondisi normal),
	// pertahankan snapshot lama.
}

const std::map<pid_t, Sys::Process>& ProcessManager::processes() {
	return m_processes;
}