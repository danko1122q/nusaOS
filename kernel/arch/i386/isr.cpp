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

	void isr_init(){
		idt_set_gate(0, (unsigned)isr0, 0x08, 0x8E);
		idt_set_gate(1, (unsigned)isr1, 0x08, 0x8E);
		idt_set_gate(2, (unsigned)isr2, 0x08, 0x8E);
		idt_set_gate(3, (unsigned)isr3, 0x08, 0x8E);
		idt_set_gate(4, (unsigned)isr4, 0x08, 0x8E);
		idt_set_gate(5, (unsigned)isr5, 0x08, 0x8E);
		idt_set_gate(6, (unsigned)isr6, 0x08, 0x8E);
		idt_set_gate(7, (unsigned)isr7, 0x08, 0x8E);
		// Special case for double-fault; we want to use a separate TSS so we can be sure we have a clean stack to work with.
		idt_set_gate(8, 0, 0x30, 0x85);
		idt_set_gate(9, (unsigned)isr9, 0x08, 0x8E);
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

		// Setup the double-fault TSS and a stack for it
		memset(&fault_tss, 0, sizeof(TSS));	
		fault_tss.ss0 = 0x10;
		fault_tss.cs = 0x08;
		fault_tss.ss = 0x10;  // ss ditetapkan sekali saja (sebelumnya duplikat di bawah)
		fault_tss.ds = 0x10;
		fault_tss.es = 0x10;
		fault_tss.fs = 0x10;
		fault_tss.gs = 0x10;
		fault_tss.eflags = 0x2;
		fault_tss.cr3 = MM.kernel_page_directory.entries_physaddr();
		// BUG FIX: setelah memset, iobp = 0 yang artinya CPU mengira I/O bitmap
		// berada di awal TSS (tumpang tindih dengan data TSS itu sendiri).
		// Harus diset ke sizeof(TSS) untuk menandakan tidak ada I/O bitmap.
		fault_tss.iobp = sizeof(TSS);
		// BUG FIX: naikkan dari PAGE_SIZE*2 (8KB) ke PAGE_SIZE*4 (16KB).
		// PANIC_NOHLT dan fungsi diagnostik membutuhkan stack yang cukup.
		fault_tss.esp0 = MM.inst().alloc_kernel_stack_region(PAGE_SIZE * 4)->end();
		fault_tss.esp = fault_tss.esp0;
		fault_tss.eip = (size_t) double_fault;
	}

	[[noreturn]] void double_fault() {
		// BUG FIX: kode diagnostik harus dijalankan SEBELUM PANIC_NOHLT.
		// Sebelumnya PANIC_NOHLT dipanggil pertama (noreturn), sehingga
		// cek stack overflow dan stacktrace di bawahnya tidak pernah berjalan sama sekali.

		// Cek apakah ini disebabkan kernel stack overflow
		if (!MM.kernel_page_directory.is_mapped(TaskManager::tss.esp, false)) {
			printf("Kernel stack overflow detected!\n");
			printf("  Crashed thread ESP: 0x%x  EBP: 0x%x  EIP: 0x%x\n",
				TaskManager::tss.esp, TaskManager::tss.ebp, TaskManager::tss.eip);
		} else {
			printf("Double fault at EIP: 0x%x  ESP: 0x%x\n",
				TaskManager::tss.eip, TaskManager::tss.esp);
		}

		KernelMapper::print_stacktrace(TaskManager::tss.ebp);

		// PANIC_NOHLT adalah [[noreturn]] - dipanggil terakhir setelah info diagnostik dicetak
		PANIC_NOHLT("DOUBLE_FAULT", "A double fault occurred. Something has gone horribly wrong.");
		__builtin_unreachable();
	}

	void handle_fault(const char* err, const char* panic_msg, uint32_t sig, ISRRegisters* regs) {
		if(!TaskManager::enabled() || TaskManager::current_thread()->is_kernel_mode() || TaskManager::is_preempting()) {
			PANIC(err, "%s\nFault %d at 0x%x", panic_msg, regs->isr_num, regs->interrupt_frame.eip);
		} else {
			TrapFrame frame { nullptr, TrapFrame::Fault, regs };
			TaskManager::current_thread()->enter_trap_frame(&frame);
			TaskManager::current_process()->kill(sig);
			TaskManager::current_thread()->exit_trap_frame();
		}
	}

	void fault_handler(ISRRegisters* regs){
		if(regs->isr_num < 32){
			switch(regs->isr_num){
				case 0:
					handle_fault("DIVIDE_BY_ZERO", "Please don't do that.", SIGILL, regs);
					break;

				case 13: //GPF
					handle_fault("GENERAL_PROTECTION_FAULT", "How did you manage to do that?", SIGILL, regs);
					break;

				case 14: //Page fault
				{
					size_t err_pos;
					asm volatile ("mov %%cr2, %0" : "=r" (err_pos));
					PageFault::Type type;
					switch (regs->err_code) {
						case FAULT_USER_READ:
						case FAULT_USER_READ_GPF:
						case FAULT_KERNEL_READ:
						case FAULT_KERNEL_READ_GPF:
							type = PageFault::Type::Read;
							break;
						case FAULT_USER_WRITE:
						case FAULT_USER_WRITE_GPF:
						case FAULT_KERNEL_WRITE:
						case FAULT_KERNEL_WRITE_GPF:
							type = PageFault::Type::Write;
							break;
						default:
							type = PageFault::Type::Unknown;
					}
					const PageFault fault { err_pos, regs, type };
					if(TaskManager::is_preempting() || fault.type == PageFault::Type::Unknown || !TaskManager::current_thread()) {
						// Never want to fault while preempting
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