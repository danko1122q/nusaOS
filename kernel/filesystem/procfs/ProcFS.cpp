/*
	This file is part of nusaOS.
	SPDX-License-Identifier: GPL-3.0-or-later
	Copyright © 2016-2026 nusaOS
*/

#include "ProcFS.h"
#include <kernel/tasking/TaskManager.h>
#include <kernel/kstd/defines.h>
#include "ProcFSInode.h"
#include "ProcFSEntry.h"
#include <kernel/tasking/Process.h>

ProcFS* ProcFS::_instance;

ProcFS& ProcFS::inst() {
	return *_instance;
}

ProcFS::ProcFS() {
	_instance = this;

	entries.push_back(ProcFSEntry(Root, 0));
	entries.push_back(ProcFSEntry(RootCurProcEntry, 0));
	entries.push_back(ProcFSEntry(RootSidProcEntry, 0));
	entries.push_back(ProcFSEntry(RootCmdLine, 0));
	entries.push_back(ProcFSEntry(RootMemInfo, 0));
	entries.push_back(ProcFSEntry(RootUptime, 0));
	entries.push_back(ProcFSEntry(RootCpuInfo, 0));
	entries.push_back(ProcFSEntry(RootLockInfo, 0));

	root_inode = kstd::make_shared<ProcFSInode>(*this, entries[0]);
}

ino_t ProcFS::id_for_entry(pid_t pid, ProcFSInodeType type) {
	return (type & 0xFu) | ((unsigned)pid << 8u);
}

ProcFSInodeType ProcFS::type_for_id(ino_t id) {
	return static_cast<ProcFSInodeType>(id & 0xFu);
}

pid_t ProcFS::pid_for_id(ino_t id) {
	return (pid_t)(id >> 8u);
}

void ProcFS::proc_add(Process* proc) {
	pid_t pid = proc->pid();
	// Cegah duplikat entry (terjadi saat exec())
	for(size_t i = 0; i < entries.size(); i++)
		if(entries[i].pid == pid) return;

	entries.push_back(ProcFSEntry(RootProcEntry, pid));
	entries.push_back(ProcFSEntry(ProcExe,       pid));
	entries.push_back(ProcFSEntry(ProcCwd,       pid));
	entries.push_back(ProcFSEntry(ProcStatus,    pid));
	entries.push_back(ProcFSEntry(ProcStacks,    pid));
	entries.push_back(ProcFSEntry(ProcVMSpace,   pid));
}

void ProcFS::proc_remove(Process* proc) {
	pid_t pid = proc->pid();

	// FIX: Hapus SEMUA entries untuk pid ini.
	//
	// Bug lama: kode hanya menghapus entry PERTAMA yang cocok lalu break.
	// proc_add() menambahkan 6 entries per proses (RootProcEntry, ProcExe,
	// ProcCwd, ProcStatus, ProcStacks, ProcVMSpace).
	// Dengan break setelah erase pertama, 5 entries sisanya bocor selamanya di
	// vector entries — tidak pernah dihapus sepanjang uptime OS.
	//
	// Dampak nyata yang menyebabkan monitor force close:
	//   1. Setiap proses yang exit meninggalkan 5 entries zombie di ProcFS.
	//   2. Saat banyak app foto/file dibuka lalu ditutup (skenario persis user),
	//      ratusan stale entries menumpuk di entries vector.
	//   3. get_inode() harus linear scan seluruh entries vector setiap akses.
	//   4. iterate_entries() untuk directory listing juga scan seluruh vector.
	//   5. Saat monitor dibuka dan membaca /proc untuk semua proses, waktu scan
	//      menjadi O(n × stale_entries) — dengan ratusan zombie entries ini bisa
	//      berakibat timeout, blocking main thread, atau OOM di kernel heap.
	//   6. Lebih parah: stale entries punya pid proses yang sudah mati.
	//      Ketika ProcFSContent::status(stale_pid) atau vmspace(stale_pid) dipanggil,
	//      TaskManager::process_for_pid() gagal dan return error — TAPI
	//      iterate_entries() masih menawarkan entry ini ke userspace, sehingga
	//      Sys::Process::get_all() di ProcessManager mencoba membaca /proc/[pid]/status
	//      untuk pid yang sudah tidak ada → read() gagal → data korup/parsial
	//      → ProcessManager mungkin crash atau return data sampah ke UI.
	//
	// FIX: Iterasi mundur (dari belakang) untuk erase semua entries milik pid ini
	// tanpa invalidasi index yang sedang diproses.
	for(size_t i = entries.size(); i-- > 0; ) {
		if(entries[i].pid == pid)
			entries.erase(i);
	}
}

char* ProcFS::name() {
	return "procfs";
}

ResultRet<kstd::Arc<Inode>> ProcFS::get_inode(ino_t id) {
	if(id == root_inode_id())
		return static_cast<kstd::Arc<Inode>>(root_inode);
	else if(id == id_for_entry(0, RootCurProcEntry))
		return static_cast<kstd::Arc<Inode>>(kstd::make_shared<ProcFSInode>(*this, ProcFSEntry(RootProcEntry, TaskManager::current_process()->pid())));
	else if(id == id_for_entry(0, RootSidProcEntry))
		return static_cast<kstd::Arc<Inode>>(kstd::make_shared<ProcFSInode>(*this, ProcFSEntry(RootProcEntry, TaskManager::current_process()->sid())));

	LOCK(lock);
	for(size_t i = 0; i < entries.size(); i++) {
		if(entries[i].dir_entry.id == id)
			return static_cast<kstd::Arc<Inode>>(kstd::make_shared<ProcFSInode>(*this, entries[i]));
	}

	return Result(-ENOENT);
}

ino_t ProcFS::root_inode_id() {
	return 1;
}

uint8_t ProcFS::fsid() {
	return PROCFS_FSID;
}