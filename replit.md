# nusaOS

Hobbyist x86 operating system, forked dan dikembangkan dari [duckOS](https://github.com/byteduck/duckOS).

## Struktur Proyek

| Direktori | Deskripsi |
|-----------|-----------|
| `/kernel` | Kernel inti — memori, scheduling, ext2/procfs/socketfs, driver |
| `/libraries` | Custom libc, libm, libui, libterm, libpond, dll |
| `/programs` | Shell (`dsh`), coreutils, NSA language toolchain, fasm, lua |
| `/services` | `init`, `pond` (window server), `dhcpclient` |
| `/toolchain` | Script build cross-compiler i686/aarch64 |
| `/scripts` | Disk image creation, QEMU boot, versioning |
| `/docs` | Dokumentasi NSA language dan app guide |

## Build

```bash
# 1. Build toolchain (sekali saja, 10-30 menit)
cd toolchain && ARCH=i686 ./build-toolchain.sh

# 2. Generate CMake toolchain file
ARCH=i686 ./build-toolchain.sh make-toolchain-file

# 3. Configure
mkdir -p ../build/i686 && cd ../build/i686
cmake ../.. -DCMAKE_TOOLCHAIN_FILE=./CMakeToolchain.txt

# 4. Build & run
make -j$(nproc) && make install && make img && make qemu
```

## Perbaikan Bug Anti-BSOD (Userspace Protection)

### Bug #1 — MemoryManager::page_fault_handler: user faults → PANIC
**File:** `kernel/memory/MemoryManager.cpp`  
**Fix:** `FAULT_USER_READ/WRITE` sekarang `kill(SIGSEGV)` ke proses, bukan BSOD.

### Bug #2 — isr.cpp: routing salah saat is_preempting()
**File:** `kernel/arch/i386/isr.cpp`  
**Fix:** Saat preempting, hanya PANIC jika fault address di kernel space. User-space fault tetap diarahkan ke handle_pagefault normal.

### Bug #3 — Thread::block(): INVALID_BLOCK & THREAD_DEADLOCK → PANIC
**File:** `kernel/tasking/Thread.cpp`  
**Fix:** User thread non-ALIVE → log+return saja. Deadlock antar user thread → kill kedua proses.

### Bug #4 — sys_threadexit: leave_critical() dengan _in_syscall=true → ASSERT
**File:** `kernel/syscall/thread.cpp`  
**Fix:** Ganti `leave_critical()` dengan `leave_syscall()` agar `_in_syscall` di-clear dulu.

### Bug #5 — sys_waitpid: ASSERT(blocker->waited_process()) → BSOD
**File:** `kernel/syscall/waitpid.cpp`  
**Fix:** Ganti ASSERT dengan `if (!waited_process) return -ECHILD`.

### Bug #6 — ptrace PEEK/POKE: ASSERT bounds check → BSOD dari input user
**File:** `kernel/syscall/ptrace.cpp`  
**Fix:** Ganti ASSERT dengan `if (obj_offset >= size) return -EFAULT`.

### Bug #7 — VMObject::clone() base: ASSERT(false) → BSOD
**File:** `kernel/memory/VMObject.cpp`  
**Fix:** Ganti dengan `return Result(ENOSYS)` + KLog error.

### Bug #8 — ThreadJoin ASSERT post-join → BSOD pada race condition
**File:** `kernel/syscall/thread.cpp`  
**Fix:** Ganti ASSERT dengan KLog warning.

### Bug #9 — TaskManager: INVALID_CONTEXT_SWITCH → BSOD saat thread di-kill
**File:** `kernel/tasking/TaskManager.cpp`  
**Fix:** Fallback ke idle thread daripada PANIC saat thread terpilih sudah mati.

## Arsitektur Kernel

- **Memory:** Paging x86, CoW fork, VMSpace/VMObject/VMRegion hierarchy
- **Tasking:** Preemptive multitasking, threading, signal handling
- **FS:** Ext2, procfs, socketfs, ptyfs
- **IPC:** Shared memory, futex, Unix sockets
- **Net:** E1000 driver, TCP/UDP/IP stack (early stage)
- **GUI:** `pond` window server via `/services/pond`
- **Lang:** NSA language (bytecode compiler + VM) via `/programs/nsa-lang`
