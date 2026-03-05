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

#include "isr.h"
#include <kernel/interrupt/interrupt.h>
#include <kernel/kstd/kstddef.h>
#include <kernel/memory/MemoryManager.h>
#include <kernel/kstd/kstdio.h>
#include "idt.h"
#include <kernel/tasking/TaskManager.h>
#include <kernel/tasking/Signal.h>
#include <kernel/tasking/Thread.h>
#include <kernel/tasking/Process.h>
#include <kernel/KernelMapper.h>
#include <kernel/arch/registers.h>

namespace Interrupt {
	TSS fault_tss;

	[[noreturn]] void double_fault();

	void isr_init() {
		idt_set_gate(0,  (unsigned)isr0,  0x08, 0x8E);
		idt_set_gate(1,  (unsigned)isr1,  0x08, 0x8E);
		idt_set_gate(2,  (unsigned)isr2,  0x08, 0x8E);
		idt_set_gate(3,  (unsigned)isr3,  0x08, 0x8E);
		idt_set_gate(4,  (unsigned)isr4,  0x08, 0x8E);
		idt_set_gate(5,  (unsigned)isr5,  0x08, 0x8E);
		idt_set_gate(6,  (unsigned)isr6,  0x08, 0x8E);
		idt_set_gate(7,  (unsigned)isr7,  0x08, 0x8E);
		// Double-fault uses a separate TSS (task gate) for a clean stack
		idt_set_gate(8,  0,               0x30, 0x85);
		idt_set_gate(9,  (unsigned)isr9,  0x08, 0x8E);
		idt_set_gate(10, (unsigned)isr10, 0x08, 0x8E);
		idt_set_gate(11, (unsigned)isr11, 0x08, 0x8E);
		idt_set_gate(12, (unsigned)isr12, 0x08, 0x8E);
		idt_set_gate(13, (unsigned)isr13, 0x08, 0x8E);
		idt_set_gate(14, (unsigned)isr14, 0x08, 0x8E);
		idt_set_gate(15, (unsigned)isr15, 0x08, 0x8E);
		idt_set_gate(16, (unsigned)isr16, 0x08, 0x8E);
		idt_set_gate(17, (unsigned)isr17, 0x08, 0x8E);
		idt_set_gate(18, (unsigned)isr18, 0x08, 0x8E);
		idt_set_gate(19, (unsigned)isr19, 0x08, 0x8E);
		idt_set_gate(20, (unsigned)isr20, 0x08, 0x8E);
		idt_set_gate(21, (unsigned)isr21, 0x08, 0x8E);
		idt_set_gate(22, (unsigned)isr22, 0x08, 0x8E);
		idt_set_gate(23, (unsigned)isr23, 0x08, 0x8E);
		idt_set_gate(24, (unsigned)isr24, 0x08, 0x8E);
		idt_set_gate(25, (unsigned)isr25, 0x08, 0x8E);
		idt_set_gate(26, (unsigned)isr26, 0x08, 0x8E);
		idt_set_gate(27, (unsigned)isr27, 0x08, 0x8E);
		idt_set_gate(28, (unsigned)isr28, 0x08, 0x8E);
		idt_set_gate(29, (unsigned)isr29, 0x08, 0x8E);
		idt_set_gate(30, (unsigned)isr30, 0x08, 0x8E);
		idt_set_gate(31, (unsigned)isr31, 0x08, 0x8E);

		// Setup the double-fault TSS
		memset(&fault_tss, 0, sizeof(TSS));
		fault_tss.ss0    = 0x10;
		fault_tss.cs     = 0x08;
		fault_tss.ss     = 0x10;
		fault_tss.ds     = 0x10;
		fault_tss.es     = 0x10;
		fault_tss.fs     = 0x10;
		fault_tss.gs     = 0x10;
		fault_tss.eflags = 0x2;
		fault_tss.cr3    = MM.kernel_page_directory.entries_physaddr();
		// iobp must point past the end of the TSS; 0 would overlap TSS fields
		fault_tss.iobp   = sizeof(TSS);
		// Allocate a 16 KiB stack so diagnostic code has plenty of room
		fault_tss.esp0   = MM.inst().alloc_kernel_stack_region(PAGE_SIZE * 4)->end();
		fault_tss.esp    = fault_tss.esp0;
		fault_tss.eip    = (size_t)double_fault;
	}

	[[noreturn]] void double_fault() {
		// --- Diagnostics FIRST, PANIC_NOHLT (noreturn) LAST ---

		if(!MM.kernel_page_directory.is_mapped(TaskManager::tss.esp, false)) {
			printf("Kernel stack overflow detected!\n");
			printf("  Crashed thread ESP: 0x%x  EBP: 0x%x  EIP: 0x%x\n",
			       TaskManager::tss.esp, TaskManager::tss.ebp, TaskManager::tss.eip);
		} else {
			printf("Double fault at EIP: 0x%x  ESP: 0x%x\n",
			       TaskManager::tss.eip, TaskManager::tss.esp);
		}

		KernelMapper::print_stacktrace(TaskManager::tss.ebp);

		PANIC_NOHLT("DOUBLE_FAULT", "A double fault occurred. Something has gone horribly wrong.");
		__builtin_unreachable();
	}

	void handle_fault(const char* err, const char* panic_msg, uint32_t sig, ISRRegisters* regs) {
		// current_thread() returns kstd::Arc<Thread>, use .get() for raw pointer checks
		auto& thread_arc = TaskManager::current_thread();
		Thread* thread = thread_arc.get();
		// Escalate to PANIC only when we truly cannot recover:
		// - TaskManager not running yet (early boot)
		// - No current thread (scheduler not started)
		// - Thread is a kernel thread (kernel bug)
		// - Mid-preemption / context switch (unsafe to reschedule)
		if (!TaskManager::enabled() || !thread || thread->is_kernel_mode() || TaskManager::is_preempting()) {
			KLog::err("fault", "Kernel fault #{} at EIP={#x} ESP={#x} | enabled={} kernel_mode={} preempting={}",
				regs->isr_num,
				regs->interrupt_frame.eip,
				regs->interrupt_frame.esp,
				TaskManager::enabled(),
				thread ? thread->is_kernel_mode() : true,
				TaskManager::is_preempting());
			PANIC(err, "%s\nFault %d at EIP=0x%x ESP=0x%x", panic_msg, regs->isr_num,
				regs->interrupt_frame.eip, regs->interrupt_frame.esp);
		} else {
			// User-mode fault: kill the process, do NOT panic
			KLog::warn("fault", "User fault #{} (sig {}) at EIP={#x} in PID {}",
				regs->isr_num, sig,
				regs->interrupt_frame.eip,
				TaskManager::current_process()->pid());
			TrapFrame frame { nullptr, TrapFrame::Fault, regs };
			thread->enter_trap_frame(&frame);
			TaskManager::current_process()->kill(sig);
			thread->exit_trap_frame();
		}
	}

	void fault_handler(ISRRegisters* regs) {
		if(regs->isr_num < 32) {
			switch(regs->isr_num) {
				case 0:
					// Division by zero → SIGFPE (correct POSIX signal)
					handle_fault("DIVIDE_BY_ZERO", "Division by zero.", SIGFPE, regs);
					break;

				case 13: // GPF
					// General protection fault → SIGSEGV (more correct than SIGILL)
					handle_fault("GENERAL_PROTECTION_FAULT", "General protection fault.", SIGSEGV, regs);
					break;

				case 14: // Page fault
				{
					size_t err_pos;
					asm volatile("mov %%cr2, %0" : "=r"(err_pos));
					// x86 page fault error code is a bitmask:
					//   bit 0: 0=not-present,        1=protection violation
					//   bit 1: 0=read,               1=write
					//   bit 2: 0=kernel access,      1=user access
					//   bit 3: 1=reserved bit set in PTE (always a hard fault)
					//   bit 4: 1=instruction fetch (NX violation)
					PageFault::Type type;
					if (regs->err_code & (1u << 3)) {
						// Reserved bit in page table — always a kernel bug, escalate
						type = PageFault::Type::Unknown;
					} else if (regs->err_code & (1u << 1)) {
						type = PageFault::Type::Write;
					} else {
						type = PageFault::Type::Read;
					}
					const PageFault fault { err_pos, regs, type };
					// Only call page_fault_handler (which may PANIC) when:
					// - mid-preemption (unsafe to reschedule), OR
					// - reserved-bit violation (always a kernel bug), OR
					// - no current thread (early boot)
					if(TaskManager::is_preempting() || fault.type == PageFault::Type::Unknown || !TaskManager::current_thread()) {
						MemoryManager::inst().page_fault_handler(regs);
					} else {
						auto thread = TaskManager::current_thread();
						TrapFrame frame { nullptr, TrapFrame::Fault, regs };
						thread->enter_trap_frame(&frame);
						thread->handle_pagefault(fault);
						thread->exit_trap_frame();
					}
					break;
				}

				default:
					handle_fault("UNKNOWN_FAULT", "What did you do?", SIGILL, regs);
			}
		}
	}
}