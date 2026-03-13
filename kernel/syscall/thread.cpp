/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2023 Byteduck */

#include "../tasking/Process.h"
#include "../memory/SafePointer.h"

int Process::sys_threadcreate(void* (*entry_func)(void* (*)(void*), void*), void* (*thread_func)(void*), void* arg) {
        auto thread = kstd::make_shared<Thread>(_self_ptr, TaskManager::get_new_pid(), entry_func, thread_func, arg);
        insert_thread(thread);
        TaskManager::queue_thread(thread);
        return thread->tid();
}

int Process::sys_gettid() {
        return TaskManager::current_thread()->tid();
}

int Process::sys_threadjoin(tid_t tid, UserspacePointer<void*> retp) {
        auto cur_thread = TaskManager::current_thread();
        auto thread = get_thread(tid);
        if(!thread) {
                // See if the thread died already.
                LOCK(_thread_lock);
                auto return_val = _thread_return_values.find_node(tid);
                if(!return_val)
                        return -ESRCH;
                if(retp)
                        retp.set(return_val->data.second);
                return SUCCESS;
        }
        Result result = cur_thread->join(cur_thread, thread, retp);
        if(result.is_success()) {
                // Jangan ASSERT — jika ada race condition thread belum mati sempurna,
                // log warning saja; proses user tidak harus BSOD karena ini.
                if (thread->state() != Thread::DEAD && !thread->_waiting_to_die)
                        KLog::warn("Thread", "threadjoin: joined thread {}(tid:{}) state={} unexpected after join",
                                thread->process()->name().c_str(), thread->tid(), thread->state_name());
                thread.reset();
        }
        return result.code();
}

int Process::sys_threadexit(void* return_value) {
        TaskManager::current_thread()->exit(return_value);
        // Panggil leave_syscall() bukan leave_critical() langsung.
        // leave_critical() dengan _in_syscall=true akan ASSERT(!_in_syscall) → BSOD.
        // leave_syscall() benar-benar set _in_syscall=false dulu sebelum leave_critical(),
        // sehingga leave_critical() bisa menemukan _waiting_to_die dan mereap thread ini.
        TaskManager::current_thread()->leave_syscall();
        // Seharusnya tidak pernah sampai sini — leave_syscall() harus yield ke thread lain.
        // Jika sampai sini, itu bug kernel yang benar-benar fatal.
        PANIC("THREADEXIT_NO_SWITCH", "sys_threadexit did not switch away from the dying thread.");
        __builtin_unreachable();
}