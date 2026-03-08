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


#include "WaitBlocker.h"
#include "TaskManager.h"
#include "Thread.h"
#include "Process.h"

kstd::vector<kstd::Weak<WaitBlocker>> WaitBlocker::blockers;
kstd::vector<WaitBlocker::Notification> WaitBlocker::unhandled_notifications;
Mutex WaitBlocker::lock {"WaitBlocker"};

kstd::Arc<WaitBlocker> WaitBlocker::make(kstd::Arc<Thread>& thread, pid_t wait_for, int options) {
	auto new_blocker = kstd::Arc<WaitBlocker>(new WaitBlocker(thread, wait_for, options));
	LOCK(WaitBlocker::lock);

	/* Drain any stale entries whose process pointer has been nulled out
	   (process was reaped before any blocker could consume the notification). */
	for (size_t i = 0; i < unhandled_notifications.size(); ) {
		if (unhandled_notifications[i].process == nullptr) {
			unhandled_notifications.erase(i);
		} else {
			i++;
		}
	}

	/* Try to satisfy immediately from pending notifications. */
	for (size_t i = 0; i < unhandled_notifications.size(); i++) {
		auto& notif = unhandled_notifications[i];
		if (new_blocker->notify(notif.process, notif.pid, notif.pgid, notif.ppid,
		                        notif.reason, notif.status)) {
			unhandled_notifications.erase(i);
			break;
		}
	}

	blockers.push_back(new_blocker);
	return new_blocker;
}

WaitBlocker::WaitBlocker(kstd::Arc<Thread>& thread, pid_t wait_for, int options):
	_ppid(thread->process()->pid()),
	_wait_pid(wait_for),
	_options(options),
	_thread(thread)
{
	if (_wait_pid < -1) {
		_wait_pgid = -_wait_pid;
	} else if (_wait_pid == 0) {
		_wait_pgid = thread->process()->pgid();
	} else {
		_wait_pgid = -1;
	}
}

bool WaitBlocker::is_ready() {
	if (_ready.load(MemoryOrder::Acquire)) {
		if (_reason == Stopped)
			return _waited_process->state() == Process::STOPPED;
		return true;
	}
	return false;
}

Process* WaitBlocker::waited_process() {
	return _waited_process;
}

pid_t WaitBlocker::error() {
	return _err;
}

int WaitBlocker::status() {
	return _status;
}

void WaitBlocker::notify_all(Process* proc, WaitBlocker::Reason reason, int status) {
	if (!proc || proc->pid() == -1)
		return;

	pid_t proc_pid  = proc->pid();
	pid_t proc_pgid = proc->pgid();
	pid_t proc_ppid = proc->ppid();

	LOCK(WaitBlocker::lock);

	for (size_t i = 0; i < blockers.size(); i++) {
		auto blocker = blockers[i].lock();
		if (!blocker) {
			blockers.erase(i);
			i--;
			continue;
		}
		if (blocker->notify(proc, proc_pid, proc_pgid, proc_ppid, reason, status)) {
			blockers.erase(i);
			return;
		}
	}

	/* No blocker was ready — store the notification for a future waitpid().
	   We store the raw pointer here; it is nulled out in notify_all_reap()
	   when the process is about to be freed so stale entries can be purged. */
	Notification notif;
	notif.process = proc;
	notif.pid     = proc_pid;
	notif.pgid    = proc_pgid;
	notif.ppid    = proc_ppid;
	notif.reason  = reason;
	notif.status  = status;
	unhandled_notifications.push_back(notif);
}

/*
 * Call this from Process::reap() BEFORE the Process object is deleted.
 * It nulls out any stored raw pointers to this process in the pending
 * notification queue so that WaitBlocker::make() can safely prune them.
 */
void WaitBlocker::notify_all_reap(Process* proc) {
	if (!proc) return;
	LOCK(WaitBlocker::lock);
	for (size_t i = 0; i < unhandled_notifications.size(); i++) {
		if (unhandled_notifications[i].process == proc)
			unhandled_notifications[i].process = nullptr;
	}
}

bool WaitBlocker::notify(Process* proc, pid_t proc_pid, pid_t proc_pgid, pid_t proc_ppid,
                         WaitBlocker::Reason reason, int status) {
	/* If the notification is for a stop, and the process has a tracer,
	   route it there first. */
	if (reason == Stopped && proc && proc->is_traced() &&
	    !proc->is_traced_by(_thread->process()))
		return false;

	if (_ready.load())
		return true;

	/* Check parentage: we only accept notifications for our children,
	   or processes we are tracing. */
	if (proc_ppid != _ppid && !(proc && proc->is_traced_by(_thread->process())))
		return false;

	/* Filter by pgid if requested. */
	if (_wait_pgid != -1 && proc_pgid != _wait_pgid)
		return false;

	/* Filter by specific pid if requested. */
	if (_wait_pid > 0 && proc_pid != _wait_pid)
		return false;

	_reason          = reason;
	_waited_process  = proc;

	switch (reason) {
		case Exited:
			_status = __WIFEXITED | (status & 0xff);
			break;
		case Signalled:
			_status = __WIFSIGNALED | (status & 0xff);
			break;
		case Stopped:
			_status = __WIFSTOPPED | (status & 0xff);
			break;
	}

	_ready.store(true, MemoryOrder::Release);
	return true;
}