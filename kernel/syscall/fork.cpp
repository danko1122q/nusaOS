/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2023 Byteduck */

#include "../tasking/Process.h"
#include "../tasking/TaskManager.h"

pid_t Process::sys_fork(ThreadRegisters& regs) {
	// FIX: Zero out eax in the registers BEFORE the child Process is created.
	// Process(to_fork, regs) copies regs into the child thread's saved state.
	// If eax still holds the syscall number (SYS_FORK=2) at that point, the
	// child will return 2 instead of 0 when it is first scheduled.
	// We save the original eax and restore it for the parent afterward so
	// that syscall_handler can still write the correct child pid into it.
#if defined(__i386__)
	auto saved_eax = regs.gp.eax;
	regs.gp.eax = 0;  // child will see fork() = 0
#endif

	auto* new_proc = new Process(this, regs);

#if defined(__i386__)
	regs.gp.eax = saved_eax;  // restore for parent — syscall_handler overwrites anyway
#endif

	// If the process execs before sys_fork finishes, pid would be -1 so we save it here
	auto pid = new_proc->pid();
	TaskManager::add_process(new_proc->_self_ptr);
	return pid;  // syscall_handler writes this into parent's regs.gp.eax
}
