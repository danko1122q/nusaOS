.
├── assets
│   └── logo.png
├── base
│   ├── etc
│   │   ├── init
│   │   │   └── services
│   │   │       ├── dhcpclient.service
│   │   │       ├── pond.service
│   │   │       └── quack.service
│   │   ├── libui.conf
│   │   └── pond.conf
│   ├── home
│   │   ├── calc.nsa
│   │   ├── demo
│   │   │   ├── demo_import.nsa
│   │   │   ├── math.nss
│   │   │   └── strutil.nss
│   │   ├── demo.nsa
│   │   ├── greet.nsa
│   │   ├── student_grades.nsa
│   │   ├── test_func.nsa
│   │   └── test_v22.nsa
│   └── usr
│       └── share
│           ├── cursors
│           │   ├── cursor.png
│           │   ├── resize_dl.png
│           │   ├── resize_dr.png
│           │   ├── resize_h.png
│           │   └── resize_v.png
│           ├── cursors2
│           │   ├── cursor.png
│           │   ├── resize_dl.png
│           │   ├── resize_dr.png
│           │   ├── resize_h.png
│           │   └── resize_v.png
│           ├── fonts
│           │   ├── gohufont-11.bdf
│           │   └── gohufont-14.bdf
│           ├── icons
│           │   ├── filetypes
│           │   │   ├── default.icon
│           │   │   │   ├── 16x16.png
│           │   │   │   └── 32x32.png
│           │   │   ├── folder.icon
│           │   │   │   ├── 16x16.png
│           │   │   │   └── 32x32.png
│           │   │   └── up.png
│           │   ├── missing_icon.icon
│           │   │   └── 16x16.png
│           │   └── nusa.icon
│           │       └── logo.png
│           ├── pictures
│           │   ├── 16x16.png
│           │   └── tanda.png
│           ├── themes
│           │   ├── default
│           │   │   └── theme.thm
│           │   ├── hotdog
│           │   │   └── theme.thm
│           │   ├── light
│           │   │   └── theme.thm
│           │   ├── vapor
│           │   │   └── theme.thm
│           │   ├── vapor_dark
│           │   │   └── theme.thm
│           │   ├── volcano
│           │   │   └── theme.thm
│           │   └── win95
│           │       └── theme.thm
│           └── wallpapers
│               ├── background.png
│               ├── kelingking.png
│               └── volcano.png
├── CMakeLists.txt
├── CODE_OF_CONDUCT.md
├── docs
│   ├── app_guide
│   │   └── app.md
│   ├── common_patterns.md
│   ├── nsa-docs-english.md
│   ├── nsa-docs-indonesia.md
│   ├── ping_test
│   │   └── ping-test
│   ├── resize_png
│   │   └── resize-png
│   └── Screenshot.png
├── guide.md
├── kernel
│   ├── api
│   │   ├── cdefs.h
│   │   ├── endian.h
│   │   ├── errno.h
│   │   ├── fcntl.h
│   │   ├── futex.h
│   │   ├── hid.h
│   │   ├── ifaddrs.h
│   │   ├── if.h
│   │   ├── in.h
│   │   ├── ioctl.h
│   │   ├── ipv4.h
│   │   ├── mmap.h
│   │   ├── net.h
│   │   ├── page_size.h
│   │   ├── poll.h
│   │   ├── ptrace.h
│   │   ├── ptrace_internal.h
│   │   ├── registers.h
│   │   ├── resource.h
│   │   ├── route.h
│   │   ├── sched.h
│   │   ├── shm.h
│   │   ├── signal.h
│   │   ├── socket.h
│   │   ├── stat.h
│   │   ├── stdarg.h
│   │   ├── stddef.h
│   │   ├── stdint.h
│   │   ├── strerror.c
│   │   ├── strerror.h
│   │   ├── tcp.h
│   │   ├── termios.h
│   │   ├── time.h
│   │   ├── types.h
│   │   ├── udp.h
│   │   ├── un.h
│   │   ├── unistd.h
│   │   ├── utsname.h
│   │   └── wait.h
│   ├── arch
│   │   ├── aarch64
│   │   │   ├── aarch64util.h
│   │   │   ├── ARMTimer.cpp
│   │   │   ├── ARMTimer.h
│   │   │   ├── asm
│   │   │   │   ├── exception.h
│   │   │   │   ├── exception.S
│   │   │   │   └── startup.S
│   │   │   ├── Device.cpp
│   │   │   ├── kernel.ld
│   │   │   ├── kstdio.cpp
│   │   │   ├── MemoryManager.cpp
│   │   │   ├── MMU.cpp
│   │   │   ├── MMU.h
│   │   │   ├── PageDirectory.cpp
│   │   │   ├── PageDirectory.h
│   │   │   ├── Processor.cpp
│   │   │   ├── Processor.h
│   │   │   ├── registers.h
│   │   │   ├── rpi
│   │   │   │   ├── DeviceInfo.cpp
│   │   │   │   ├── DeviceInfo.h
│   │   │   │   ├── Framebuffer.cpp
│   │   │   │   ├── Framebuffer.h
│   │   │   │   ├── GPIO.cpp
│   │   │   │   ├── GPIO.h
│   │   │   │   ├── Mailbox.cpp
│   │   │   │   ├── Mailbox.h
│   │   │   │   ├── MiniUART.cpp
│   │   │   │   ├── MiniUART.h
│   │   │   │   ├── MMIO.cpp
│   │   │   │   └── MMIO.h
│   │   │   ├── startup.cpp
│   │   │   ├── tasking.cpp
│   │   │   └── tasking.h
│   │   ├── i386
│   │   │   ├── asm
│   │   │   │   ├── gdt.s
│   │   │   │   ├── int.s
│   │   │   │   ├── startup.s
│   │   │   │   ├── syscall.s
│   │   │   │   ├── tasking.s
│   │   │   │   └── timing.s
│   │   │   ├── CPUID.h
│   │   │   ├── device
│   │   │   │   ├── AC97Device.cpp
│   │   │   │   ├── AC97Device.h
│   │   │   │   ├── BochsVGADevice.cpp
│   │   │   │   ├── BochsVGADevice.h
│   │   │   │   ├── Device.cpp
│   │   │   │   ├── PATADevice.cpp
│   │   │   │   ├── PATADevice.h
│   │   │   │   ├── ResolutionMenu.cpp
│   │   │   │   └── ResolutionMenu.h
│   │   │   ├── gdt.cpp
│   │   │   ├── gdt.h
│   │   │   ├── idt.cpp
│   │   │   ├── idt.h
│   │   │   ├── irq.cpp
│   │   │   ├── irq.h
│   │   │   ├── isr.cpp
│   │   │   ├── isr.h
│   │   │   ├── kernel.ld
│   │   │   ├── kstdio.cpp
│   │   │   ├── MemoryManager.cpp
│   │   │   ├── PageDirectory.cpp
│   │   │   ├── PageDirectory.h
│   │   │   ├── PageTable.cpp
│   │   │   ├── PageTable.h
│   │   │   ├── Processor.cpp
│   │   │   ├── Processor.h
│   │   │   ├── registers.h
│   │   │   ├── startup.cpp
│   │   │   ├── tasking.cpp
│   │   │   ├── tasking.h
│   │   │   └── time
│   │   │       ├── CMOS.cpp
│   │   │       ├── CMOS.h
│   │   │       ├── PIT.cpp
│   │   │       ├── PIT.h
│   │   │       ├── RTC.cpp
│   │   │       └── RTC.h
│   │   ├── Processor.h
│   │   ├── registers.h
│   │   └── tasking.h
│   ├── Atomic.h
│   ├── bootlogo.h
│   ├── CMakeLists.txt
│   ├── CommandLine.cpp
│   ├── CommandLine.h
│   ├── constructors.cpp
│   ├── constructors.h
│   ├── device
│   │   ├── ATA.h
│   │   ├── BlockDevice.cpp
│   │   ├── BlockDevice.h
│   │   ├── CharacterDevice.cpp
│   │   ├── CharacterDevice.h
│   │   ├── Device.cpp
│   │   ├── Device.h
│   │   ├── DiskDevice.cpp
│   │   ├── DiskDevice.h
│   │   ├── I8042.cpp
│   │   ├── I8042.h
│   │   ├── KernelLogDevice.cpp
│   │   ├── KernelLogDevice.h
│   │   ├── KeyboardDevice.cpp
│   │   ├── KeyboardDevice.h
│   │   ├── MouseDevice.cpp
│   │   ├── MouseDevice.h
│   │   ├── MultibootVGADevice.cpp
│   │   ├── MultibootVGADevice.h
│   │   ├── NullDevice.cpp
│   │   ├── NullDevice.h
│   │   ├── PartitionDevice.cpp
│   │   ├── PartitionDevice.h
│   │   ├── RandomDevice.cpp
│   │   ├── RandomDevice.h
│   │   ├── VGADevice.cpp
│   │   ├── VGADevice.h
│   │   ├── ZeroDevice.cpp
│   │   └── ZeroDevice.h
│   ├── filesystem
│   │   ├── DirectoryEntry.cpp
│   │   ├── DirectoryEntry.h
│   │   ├── ext2
│   │   │   ├── Ext2BlockGroup.cpp
│   │   │   ├── Ext2BlockGroup.h
│   │   │   ├── Ext2Filesystem.cpp
│   │   │   ├── Ext2Filesystem.h
│   │   │   ├── Ext2.h
│   │   │   ├── Ext2Inode.cpp
│   │   │   └── Ext2Inode.h
│   │   ├── FileBasedFilesystem.cpp
│   │   ├── FileBasedFilesystem.h
│   │   ├── File.cpp
│   │   ├── FileDescriptor.cpp
│   │   ├── FileDescriptor.h
│   │   ├── File.h
│   │   ├── Filesystem.cpp
│   │   ├── Filesystem.h
│   │   ├── Inode.cpp
│   │   ├── InodeFile.cpp
│   │   ├── InodeFile.h
│   │   ├── Inode.h
│   │   ├── InodeMetadata.cpp
│   │   ├── InodeMetadata.h
│   │   ├── LinkedInode.cpp
│   │   ├── LinkedInode.h
│   │   ├── Pipe.cpp
│   │   ├── Pipe.h
│   │   ├── procfs
│   │   │   ├── ProcFSContent.cpp
│   │   │   ├── ProcFSContent.h
│   │   │   ├── ProcFS.cpp
│   │   │   ├── ProcFSEntry.cpp
│   │   │   ├── ProcFSEntry.h
│   │   │   ├── ProcFS.h
│   │   │   ├── ProcFSInode.cpp
│   │   │   ├── ProcFSInode.h
│   │   │   └── ProcFSInodeType.h
│   │   ├── ptyfs
│   │   │   ├── PTYFS.cpp
│   │   │   ├── PTYFS.h
│   │   │   ├── PTYFSInode.cpp
│   │   │   └── PTYFSInode.h
│   │   ├── socketfs
│   │   │   ├── SocketFSClient.h
│   │   │   ├── SocketFS.cpp
│   │   │   ├── socketfs_defines.h
│   │   │   ├── SocketFS.h
│   │   │   ├── SocketFSInode.cpp
│   │   │   └── SocketFSInode.h
│   │   ├── VFS.cpp
│   │   └── VFS.h
│   ├── font8x8
│   │   ├── font8x8_basic.h
│   │   ├── font8x8_block.h
│   │   ├── font8x8_box.h
│   │   ├── font8x8_control.h
│   │   ├── font8x8_ext_latin.h
│   │   ├── font8x8_greek.h
│   │   ├── font8x8.h
│   │   ├── font8x8_hiragana.h
│   │   ├── font8x8_latin.h
│   │   ├── font8x8_misc.h
│   │   └── font8x8_sga.h
│   ├── interrupt
│   │   ├── interrupt.h
│   │   ├── IRQHandler.cpp
│   │   └── IRQHandler.h
│   ├── IO.cpp
│   ├── IO.h
│   ├── kernel_c.cpp
│   ├── kernel_c.h
│   ├── KernelMapper.cpp
│   ├── KernelMapper.h
│   ├── keyboard.cpp
│   ├── keyboard.h
│   ├── kmain.cpp
│   ├── kmain.h
│   ├── kstd
│   │   ├── Arc.h
│   │   ├── Bitmap.h
│   │   ├── bits
│   │   │   ├── RefCount.cpp
│   │   │   └── RefCount.h
│   │   ├── circular_queue.hpp
│   │   ├── cstring.cpp
│   │   ├── cstring.h
│   │   ├── defines.h
│   │   ├── Function.h
│   │   ├── icxxabi.cpp
│   │   ├── icxxabi.h
│   │   ├── Iteration.h
│   │   ├── Iterator.h
│   │   ├── KLog.cpp
│   │   ├── KLog.h
│   │   ├── kstddef.cpp
│   │   ├── kstddef.h
│   │   ├── kstdio.cpp
│   │   ├── kstdio.h
│   │   ├── kstdlib.cpp
│   │   ├── kstdlib.h
│   │   ├── ListQueue.h
│   │   ├── LRUCache.h
│   │   ├── map.hpp
│   │   ├── Optional.cpp
│   │   ├── Optional.h
│   │   ├── pair.hpp
│   │   ├── queue.hpp
│   │   ├── string.cpp
│   │   ├── string.h
│   │   ├── types.h
│   │   ├── type_traits.h
│   │   ├── unix_types.h
│   │   ├── utility.h
│   │   └── vector.hpp
│   ├── memory
│   │   ├── AnonymousVMObject.cpp
│   │   ├── AnonymousVMObject.h
│   │   ├── BuddyZone.cpp
│   │   ├── BuddyZone.h
│   │   ├── Bytes.cpp
│   │   ├── Bytes.h
│   │   ├── InodeVMObject.cpp
│   │   ├── InodeVMObject.h
│   │   ├── KBuffer.cpp
│   │   ├── KBuffer.h
│   │   ├── kliballoc.h
│   │   ├── liballoc.cpp
│   │   ├── Memory.cpp
│   │   ├── Memory.h
│   │   ├── MemoryManager.cpp
│   │   ├── MemoryManager.h
│   │   ├── PageDirectory.cpp
│   │   ├── PageDirectory.h
│   │   ├── PhysicalPage.cpp
│   │   ├── PhysicalPage.h
│   │   ├── PhysicalRegion.cpp
│   │   ├── PhysicalRegion.h
│   │   ├── SafePointer.h
│   │   ├── Stack.h
│   │   ├── VMObject.cpp
│   │   ├── VMObject.h
│   │   ├── VMRegion.cpp
│   │   ├── VMRegion.h
│   │   ├── VMSpace.cpp
│   │   └── VMSpace.h
│   ├── multiboot.h
│   ├── net
│   │   ├── ARP.h
│   │   ├── E1000Adapter.cpp
│   │   ├── E1000Adapter.h
│   │   ├── ICMP.h
│   │   ├── ICMPSocket.cpp
│   │   ├── ICMPSocket.h
│   │   ├── IPSocket.cpp
│   │   ├── IPSocket.h
│   │   ├── NetworkAdapter.cpp
│   │   ├── NetworkAdapter.h
│   │   ├── NetworkManager.cpp
│   │   ├── NetworkManager.h
│   │   ├── Router.cpp
│   │   ├── Router.h
│   │   ├── Socket.cpp
│   │   ├── Socket.h
│   │   ├── TCPSocket.cpp
│   │   ├── TCPSocket.h
│   │   ├── UDPSocket.cpp
│   │   └── UDPSocket.h
│   ├── pci
│   │   ├── PCI.cpp
│   │   └── PCI.h
│   ├── power.c
│   ├── power.h
│   ├── random.cpp
│   ├── random.h
│   ├── Result.cpp
│   ├── Result.hpp
│   ├── StackWalker.cpp
│   ├── StackWalker.h
│   ├── syscall
│   │   ├── access.cpp
│   │   ├── chdir.cpp
│   │   ├── chmod.cpp
│   │   ├── dup.cpp
│   │   ├── exec.cpp
│   │   ├── exit.cpp
│   │   ├── fork.cpp
│   │   ├── futex.cpp
│   │   ├── getcwd.cpp
│   │   ├── gettimeofday.cpp
│   │   ├── ioctl.cpp
│   │   ├── isatty.cpp
│   │   ├── kill.cpp
│   │   ├── link.cpp
│   │   ├── mem.cpp
│   │   ├── mkdir.cpp
│   │   ├── pid.cpp
│   │   ├── pipe.cpp
│   │   ├── poll.cpp
│   │   ├── ptrace.cpp
│   │   ├── ptsname.cpp
│   │   ├── read_write.cpp
│   │   ├── sigaction.cpp
│   │   ├── sleep.cpp
│   │   ├── socket.cpp
│   │   ├── stat.cpp
│   │   ├── syscall.cpp
│   │   ├── syscall.h
│   │   ├── syscall_numbers.h
│   │   ├── thread.cpp
│   │   ├── truncate.cpp
│   │   ├── uname.cpp
│   │   └── waitpid.cpp
│   ├── tasking
│   │   ├── Blocker.cpp
│   │   ├── Blocker.h
│   │   ├── BooleanBlocker.cpp
│   │   ├── BooleanBlocker.h
│   │   ├── ELF.cpp
│   │   ├── ELF.h
│   │   ├── FileBlockers.cpp
│   │   ├── FileBlockers.h
│   │   ├── Futex.cpp
│   │   ├── Futex.h
│   │   ├── JoinBlocker.cpp
│   │   ├── JoinBlocker.h
│   │   ├── Lock.cpp
│   │   ├── Lock.h
│   │   ├── Mutex.cpp
│   │   ├── Mutex.h
│   │   ├── PollBlocker.cpp
│   │   ├── PollBlocker.h
│   │   ├── ProcessArgs.cpp
│   │   ├── ProcessArgs.h
│   │   ├── Process.cpp
│   │   ├── Process.h
│   │   ├── Reaper.cpp
│   │   ├── Reaper.h
│   │   ├── Signal.cpp
│   │   ├── Signal.h
│   │   ├── SleepBlocker.cpp
│   │   ├── SleepBlocker.h
│   │   ├── TaskManager.cpp
│   │   ├── TaskManager.h
│   │   ├── Thread.cpp
│   │   ├── Thread.h
│   │   ├── Tracer.cpp
│   │   ├── Tracer.h
│   │   ├── TSS.h
│   │   ├── WaitBlocker.cpp
│   │   └── WaitBlocker.h
│   ├── terminal
│   │   ├── PTYControllerDevice.cpp
│   │   ├── PTYControllerDevice.h
│   │   ├── PTYDevice.cpp
│   │   ├── PTYDevice.h
│   │   ├── PTYMuxDevice.cpp
│   │   ├── PTYMuxDevice.h
│   │   ├── TTYDevice.cpp
│   │   ├── TTYDevice.h
│   │   ├── VirtualTTY.cpp
│   │   └── VirtualTTY.h
│   ├── tests
│   │   ├── KernelTest.cpp
│   │   ├── KernelTest.h
│   │   ├── kstd
│   │   │   ├── TestArc.cpp
│   │   │   └── TestMap.cpp
│   │   └── TestMemory.cpp
│   ├── time
│   │   ├── Time.cpp
│   │   ├── Time.h
│   │   ├── TimeKeeper.cpp
│   │   ├── TimeKeeper.h
│   │   ├── TimeManager.cpp
│   │   └── TimeManager.h
│   ├── User.cpp
│   ├── User.h
│   ├── VMWare.cpp
│   └── VMWare.h
├── libraries
│   ├── CMakeLists.txt
│   ├── ld
│   │   ├── CMakeLists.txt
│   │   ├── ld.cpp
│   │   ├── ld-nusaos.ld
│   │   └── main.S
│   ├── lib3d
│   │   ├── Buffer2D.h
│   │   ├── BufferSet.h
│   │   ├── CMakeLists.txt
│   │   ├── MatrixUtil.cpp
│   │   ├── MatrixUtil.h
│   │   ├── ObjReader.cpp
│   │   ├── ObjReader.h
│   │   ├── RenderContext.cpp
│   │   ├── RenderContext.h
│   │   ├── Texture.cpp
│   │   ├── Texture.h
│   │   ├── Vertex.h
│   │   ├── ViewportWidget.cpp
│   │   └── ViewportWidget.h
│   ├── libapp
│   │   ├── App.cpp
│   │   ├── App.h
│   │   └── CMakeLists.txt
│   ├── libc
│   │   ├── arpa
│   │   │   ├── inet.cpp
│   │   │   └── inet.h
│   │   ├── assert.c
│   │   ├── assert.h
│   │   ├── CMakeLists.txt
│   │   ├── complex.h
│   │   ├── crt0.c
│   │   ├── crti.S
│   │   ├── crtn.S
│   │   ├── ctype.c
│   │   ├── ctype.h
│   │   ├── cxxabi.c
│   │   ├── dirent.c
│   │   ├── dirent.h
│   │   ├── dlfcn.cpp
│   │   ├── dlfcn.h
│   │   ├── DNS.c
│   │   ├── DNS.h
│   │   ├── endian.h
│   │   ├── errno.c
│   │   ├── errno.h
│   │   ├── fcntl.c
│   │   ├── fcntl.h
│   │   ├── fenv.h
│   │   ├── float.h
│   │   ├── ifaddrs.c
│   │   ├── ifaddrs.h
│   │   ├── inttypes.h
│   │   ├── iso646.h
│   │   ├── libgen.cpp
│   │   ├── libgen.h
│   │   ├── limits.h
│   │   ├── locale.c
│   │   ├── locale.h
│   │   ├── math.c
│   │   ├── math.h
│   │   ├── memory.h
│   │   ├── net
│   │   │   ├── if.h
│   │   │   └── route.h
│   │   ├── netinet
│   │   │   └── in.h
│   │   ├── poll.c
│   │   ├── poll.h
│   │   ├── pthread.cpp
│   │   ├── pthread.h
│   │   ├── sched.c
│   │   ├── sched.h
│   │   ├── setjmp.h
│   │   ├── setjmp.S
│   │   ├── signal.c
│   │   ├── signal.h
│   │   ├── stdalign.h
│   │   ├── stdarg.h
│   │   ├── stdatomic.h
│   │   ├── stdbool.h
│   │   ├── stdint.h
│   │   ├── stdio.c
│   │   ├── stdio.h
│   │   ├── stdlib.c
│   │   ├── stdlib.h
│   │   ├── stdnoreturn.h
│   │   ├── string.c
│   │   ├── string.h
│   │   ├── strings.c
│   │   ├── strings.h
│   │   ├── sys
│   │   │   ├── cdefs.h
│   │   │   ├── futex.c
│   │   │   ├── futex.h
│   │   │   ├── input.h
│   │   │   ├── internals.h
│   │   │   ├── ioctl.c
│   │   │   ├── ioctl.h
│   │   │   ├── keyboard.h
│   │   │   ├── liballoc.cpp
│   │   │   ├── liballoc.h
│   │   │   ├── mman.c
│   │   │   ├── mman.h
│   │   │   ├── param.h
│   │   │   ├── printf.c
│   │   │   ├── printf.h
│   │   │   ├── ptrace.c
│   │   │   ├── ptrace.h
│   │   │   ├── reboot.cpp
│   │   │   ├── reboot.h
│   │   │   ├── registers.h
│   │   │   ├── resource.c
│   │   │   ├── resource.h
│   │   │   ├── scanf.c
│   │   │   ├── scanf.h
│   │   │   ├── shm.c
│   │   │   ├── shm.h
│   │   │   ├── socket.c
│   │   │   ├── socketfs.c
│   │   │   ├── socketfs.h
│   │   │   ├── socket.h
│   │   │   ├── stat.c
│   │   │   ├── stat.h
│   │   │   ├── status.c
│   │   │   ├── status.h
│   │   │   ├── syscall.c
│   │   │   ├── syscall.h
│   │   │   ├── thread.cpp
│   │   │   ├── thread.h
│   │   │   ├── time.h
│   │   │   ├── types.h
│   │   │   ├── un.h
│   │   │   ├── utsname.c
│   │   │   ├── utsname.h
│   │   │   ├── wait.c
│   │   │   └── wait.h
│   │   ├── termios.c
│   │   ├── termios.h
│   │   ├── tgmath.h
│   │   ├── threads.h
│   │   ├── time.cpp
│   │   ├── time.h
│   │   ├── uchar.h
│   │   ├── unistd.c
│   │   ├── unistd.h
│   │   ├── utime.c
│   │   ├── utime.h
│   │   ├── wchar.h
│   │   └── wctype.h
│   ├── libdebug
│   │   ├── CMakeLists.txt
│   │   ├── Debugger.cpp
│   │   ├── Debugger.h
│   │   ├── Info.cpp
│   │   ├── Info.h
│   │   ├── LiveDebugger.cpp
│   │   └── LiveDebugger.h
│   ├── libexec
│   │   ├── CMakeLists.txt
│   │   ├── dlfunc.cpp
│   │   ├── dlfunc.h
│   │   ├── elf.h
│   │   ├── Loader.cpp
│   │   ├── Loader.h
│   │   ├── Object.cpp
│   │   └── Object.h
│   ├── libgraphics
│   │   ├── CMakeLists.txt
│   │   ├── Color.h
│   │   ├── Deflate.cpp
│   │   ├── Deflate.h
│   │   ├── Font.cpp
│   │   ├── Font.h
│   │   ├── Framebuffer.cpp
│   │   ├── Framebuffer.h
│   │   ├── Geometry.cpp
│   │   ├── Geometry.h
│   │   ├── Graphics.cpp
│   │   ├── Graphics.h
│   │   ├── Image.cpp
│   │   ├── Image.h
│   │   ├── JPEG.cpp
│   │   ├── JPEG.h
│   │   ├── Memory.h
│   │   ├── PNG.cpp
│   │   └── PNG.h
│   ├── libkeyboard
│   │   ├── CMakeLists.txt
│   │   ├── Keyboard.cpp
│   │   └── Keyboard.h
│   ├── libmatrix
│   │   ├── Matrix.h
│   │   └── Vec.h
│   ├── libnusa
│   │   ├── Args.cpp
│   │   ├── Args.h
│   │   ├── AtomicCircularQueue.h
│   │   ├── bits
│   │   │   └── IOBits.h
│   │   ├── Buffer.h
│   │   ├── ByteBuffer.cpp
│   │   ├── ByteBuffer.h
│   │   ├── CMakeLists.txt
│   │   ├── Config.cpp
│   │   ├── Config.h
│   │   ├── DataSize.cpp
│   │   ├── DataSize.h
│   │   ├── DirectoryEntry.cpp
│   │   ├── DirectoryEntry.h
│   │   ├── File.cpp
│   │   ├── File.h
│   │   ├── FileStream.cpp
│   │   ├── FileStream.h
│   │   ├── Filesystem.h
│   │   ├── FormatStream.cpp
│   │   ├── FormatStream.h
│   │   ├── Log.cpp
│   │   ├── Log.h
│   │   ├── MappedBuffer.cpp
│   │   ├── MappedBuffer.h
│   │   ├── Object.cpp
│   │   ├── Object.h
│   │   ├── Path.cpp
│   │   ├── Path.h
│   │   ├── Result.cpp
│   │   ├── Result.h
│   │   ├── Serializable.cpp
│   │   ├── Serializable.h
│   │   ├── serialization_utils.h
│   │   ├── SharedBuffer.cpp
│   │   ├── SharedBuffer.h
│   │   ├── Socket.cpp
│   │   ├── Socket.h
│   │   ├── SpinLock.cpp
│   │   ├── SpinLock.h
│   │   ├── Stream.cpp
│   │   ├── Stream.h
│   │   ├── StringStream.cpp
│   │   ├── StringStream.h
│   │   ├── StringUtils.h
│   │   ├── Time.cpp
│   │   └── Time.h
│   ├── libpond
│   │   ├── CMakeLists.txt
│   │   ├── Context.cpp
│   │   ├── Context.h
│   │   ├── Cursor.cpp
│   │   ├── Cursor.h
│   │   ├── enums.h
│   │   ├── Event.cpp
│   │   ├── Event.h
│   │   ├── packet.cpp
│   │   ├── packet.h
│   │   ├── pond.h
│   │   ├── Window.cpp
│   │   └── Window.h
│   ├── libriver
│   │   ├── BusConnection.cpp
│   │   ├── BusConnection.h
│   │   ├── BusServer.cpp
│   │   ├── BusServer.h
│   │   ├── CMakeLists.txt
│   │   ├── Endpoint.cpp
│   │   ├── Endpoint.h
│   │   ├── Function.hpp
│   │   ├── IPCBuffer.cpp
│   │   ├── IPCBuffer.h
│   │   ├── Message.hpp
│   │   ├── packet.cpp
│   │   ├── packet.h
│   │   ├── river.h
│   │   └── SerializedString.hpp
│   ├── libsound
│   │   ├── CMakeLists.txt
│   │   ├── Connection.cpp
│   │   ├── Connection.h
│   │   ├── SampleBuffer.cpp
│   │   ├── SampleBuffer.h
│   │   ├── Sample.h
│   │   ├── Sound.cpp
│   │   ├── Sound.h
│   │   ├── SoundSource.cpp
│   │   ├── SoundSource.h
│   │   ├── WavReader.cpp
│   │   └── WavReader.h
│   ├── libsys
│   │   ├── CMakeLists.txt
│   │   ├── CPU.cpp
│   │   ├── CPU.h
│   │   ├── Memory.cpp
│   │   ├── Memory.h
│   │   ├── Process.cpp
│   │   └── Process.h
│   ├── libterm
│   │   ├── CMakeLists.txt
│   │   ├── Line.cpp
│   │   ├── Line.h
│   │   ├── Listener.h
│   │   ├── Terminal.cpp
│   │   ├── Terminal.h
│   │   └── types.h
│   ├── libtui
│   │   ├── CMakeLists.txt
│   │   ├── LineEditor.cpp
│   │   └── LineEditor.h
│   └── libui
│       ├── bits
│       │   ├── FilePicker.cpp
│       │   └── FilePicker.h
│       ├── CMakeLists.txt
│       ├── DrawContext.cpp
│       ├── DrawContext.h
│       ├── libui.cpp
│       ├── libui.h
│       ├── Menu.cpp
│       ├── Menu.h
│       ├── Poll.h
│       ├── TextLayout.cpp
│       ├── TextLayout.h
│       ├── TextStorage.cpp
│       ├── TextStorage.h
│       ├── Theme.cpp
│       ├── Theme.h
│       ├── Timer.cpp
│       ├── Timer.h
│       ├── UIException.cpp
│       ├── UIException.h
│       ├── widget
│       │   ├── Button.cpp
│       │   ├── Button.h
│       │   ├── Cell.cpp
│       │   ├── Cell.h
│       │   ├── Checkbox.cpp
│       │   ├── Checkbox.h
│       │   ├── ContainerView.cpp
│       │   ├── ContainerView.h
│       │   ├── files
│       │   │   ├── FileGridView.cpp
│       │   │   ├── FileGridView.h
│       │   │   ├── FileNavigationBar.cpp
│       │   │   ├── FileNavigationBar.h
│       │   │   ├── FileViewBase.cpp
│       │   │   ├── FileViewBase.h
│       │   │   └── FileViewDelegate.h
│       │   ├── Image.cpp
│       │   ├── Image.h
│       │   ├── Label.cpp
│       │   ├── Label.h
│       │   ├── layout
│       │   │   ├── BoxLayout.cpp
│       │   │   ├── BoxLayout.h
│       │   │   ├── FlexLayout.cpp
│       │   │   ├── FlexLayout.h
│       │   │   ├── GridLayout.cpp
│       │   │   └── GridLayout.h
│       │   ├── ListView.cpp
│       │   ├── ListView.h
│       │   ├── MenuBar.cpp
│       │   ├── MenuBar.h
│       │   ├── MenuWidget.cpp
│       │   ├── MenuWidget.h
│       │   ├── NamedCell.cpp
│       │   ├── NamedCell.h
│       │   ├── ProgressBar.cpp
│       │   ├── ProgressBar.h
│       │   ├── ScrollView.cpp
│       │   ├── ScrollView.h
│       │   ├── Stack.cpp
│       │   ├── Stack.h
│       │   ├── TableView.cpp
│       │   ├── TableView.h
│       │   ├── TextView.cpp
│       │   ├── TextView.h
│       │   ├── Widget.cpp
│       │   └── Widget.h
│       ├── Window.cpp
│       └── Window.h
├── LICENSE.txt
├── programs
│   ├── applications
│   │   ├── 3demo
│   │   │   ├── CMakeLists.txt
│   │   │   ├── DemoWidget.cpp
│   │   │   ├── DemoWidget.h
│   │   │   ├── main.cpp
│   │   │   └── resources
│   │   │       ├── app.conf
│   │   │       ├── cube.obj
│   │   │       └── icon
│   │   │           ├──  16x16.png
│   │   │           └── 32x32.png
│   │   ├── 4inarow
│   │   │   ├── 4inarow.cpp
│   │   │   ├── 4inarow.h
│   │   │   ├── CMakeLists.txt
│   │   │   ├── GameWidget.cpp
│   │   │   ├── GameWidget.h
│   │   │   ├── main.cpp
│   │   │   └── resources
│   │   │       ├── app.conf
│   │   │       └── icon
│   │   │           └── 16x16.png
│   │   ├── about
│   │   │   ├── CMakeLists.txt
│   │   │   ├── main.cpp
│   │   │   └── resources
│   │   │       ├── app.conf
│   │   │       └── icon
│   │   │           └── 16x16.png
│   │   ├── calculator
│   │   │   ├── CalculatorWidget.cpp
│   │   │   ├── CalculatorWidget.h
│   │   │   ├── CMakeLists.txt
│   │   │   ├── main.cpp
│   │   │   └── resources
│   │   │       ├── app.conf
│   │   │       └── icon
│   │   │           ├── 16x16.png
│   │   │           └── 32x32.png
│   │   ├── clock
│   │   │   ├── ClockWidget.cpp
│   │   │   ├── ClockWidget.h
│   │   │   ├── CMakeLists.txt
│   │   │   ├── main.cpp
│   │   │   └── resources
│   │   │       ├── app.conf
│   │   │       └── icon
│   │   │           └── 32x32.png
│   │   ├── CMakeLists.txt
│   │   ├── desktop
│   │   │   ├── CMakeLists.txt
│   │   │   ├── Desktop.cpp
│   │   │   ├── Desktop.h
│   │   │   ├── DesktopWidget.cpp
│   │   │   ├── DesktopWidget.h
│   │   │   ├── main.cpp
│   │   │   └── resources
│   │   │       ├── app.conf
│   │   │       └── icon
│   │   │           └── 16x16.png
│   │   ├── ducksweeper
│   │   │   ├── CMakeLists.txt
│   │   │   ├── Ducksweeper.cpp
│   │   │   ├── Ducksweeper.h
│   │   │   ├── ElapsedWidget.cpp
│   │   │   ├── ElapsedWidget.h
│   │   │   ├── GameWidget.cpp
│   │   │   ├── GameWidget.h
│   │   │   ├── main.cpp
│   │   │   └── resources
│   │   │       ├── app.conf
│   │   │       ├── flag.png
│   │   │       ├── icon
│   │   │       │   └── 16x16.png
│   │   │       └── nusa.png
│   │   ├── editor
│   │   │   ├── CMakeLists.txt
│   │   │   ├── main.cpp
│   │   │   └── resources
│   │   │       ├── app.conf
│   │   │       ├── filetypes
│   │   │       │   ├── nsa.icon
│   │   │       │   │   └── 32x32.png
│   │   │       │   └── text.icon
│   │   │       │       ├── 16x16.png
│   │   │       │       └── 32x32.png
│   │   │       └── icon
│   │   │           ├── 16x16.png
│   │   │           └── 32x32.png
│   │   ├── files
│   │   │   ├── CMakeLists.txt
│   │   │   ├── main.cpp
│   │   │   └── resources
│   │   │       ├── app.conf
│   │   │       └── icon
│   │   │           ├── 16x16.png
│   │   │           └── 32x32.png
│   │   ├── flappybird
│   │   │   ├── CMakeLists.txt
│   │   │   ├── FlappyBird.cpp
│   │   │   ├── FlappyBird.h
│   │   │   ├── GameWidget.cpp
│   │   │   ├── GameWidget.h
│   │   │   ├── main.cpp
│   │   │   └── resources
│   │   │       ├── app.conf
│   │   │       └── icon
│   │   │           └── 32x32.png
│   │   ├── hex
│   │   │   ├── CMakeLists.txt
│   │   │   ├── HexEditor.cpp
│   │   │   ├── HexEditor.h
│   │   │   ├── HexWidget.cpp
│   │   │   ├── HexWidget.h
│   │   │   ├── main.cpp
│   │   │   ├── resources
│   │   │   │   ├── app.conf
│   │   │   │   └── icon
│   │   │   │       └── 32x32.png
│   │   │   ├── StatusBar.cpp
│   │   │   └── StatusBar.h
│   │   ├── monitor
│   │   │   ├── AppListWidget.cpp
│   │   │   ├── AppListWidget.h
│   │   │   ├── CMakeLists.txt
│   │   │   ├── CpuGraphWidget.cpp
│   │   │   ├── CpuGraphWidget.h
│   │   │   ├── main.cpp
│   │   │   ├── MemGraphWidget.cpp
│   │   │   ├── MemGraphWidget.h
│   │   │   └── resources
│   │   │       ├── app.conf
│   │   │       └── icon
│   │   │           ├── 16x16.png
│   │   │           └── 32x32.png
│   │   ├── sandbar
│   │   │   ├── AppMenu.cpp
│   │   │   ├── AppMenu.h
│   │   │   ├── CMakeLists.txt
│   │   │   ├── main.cpp
│   │   │   ├── modules
│   │   │   │   ├── CPUModule.cpp
│   │   │   │   ├── CPUModule.h
│   │   │   │   ├── GraphModule.cpp
│   │   │   │   ├── GraphModule.h
│   │   │   │   ├── MemoryModule.cpp
│   │   │   │   ├── MemoryModule.h
│   │   │   │   ├── Module.h
│   │   │   │   ├── TimeModule.cpp
│   │   │   │   └── TimeModule.h
│   │   │   ├── resources
│   │   │   │   ├── app.conf
│   │   │   │   └── icon
│   │   │   │       ├── 16x16.png
│   │   │   │       └── 32x32.png
│   │   │   ├── Sandbar.cpp
│   │   │   ├── Sandbar.h
│   │   │   ├── SandbarWidget.cpp
│   │   │   └── SandbarWidget.h
│   │   ├── terminal
│   │   │   ├── CMakeLists.txt
│   │   │   ├── main.cpp
│   │   │   ├── resources
│   │   │   │   ├── app.conf
│   │   │   │   └── icon
│   │   │   │       ├── 16x16.png
│   │   │   │       └── 32x32.png
│   │   │   ├── TerminalWidget.cpp
│   │   │   └── TerminalWidget.h
│   │   ├── tetris
│   │   │   ├── CMakeLists.txt
│   │   │   ├── main.cpp
│   │   │   ├── readme.md
│   │   │   ├── resources
│   │   │   │   ├── app.conf
│   │   │   │   └── icon
│   │   │   │       └── 32x32.png
│   │   │   ├── TetrisWidget.cpp
│   │   │   └── TetrisWidget.h
│   │   ├── uxn
│   │   │   ├── CMakeLists.txt
│   │   │   ├── devices
│   │   │   │   ├── ConsoleDevice.cpp
│   │   │   │   ├── ConsoleDevice.h
│   │   │   │   ├── Device.cpp
│   │   │   │   ├── Device.h
│   │   │   │   ├── ScreenDevice.cpp
│   │   │   │   └── ScreenDevice.h
│   │   │   ├── main.cpp
│   │   │   ├── resources
│   │   │   │   └── app.conf
│   │   │   ├── Uxn.cpp
│   │   │   └── Uxn.h
│   │   └── viewer
│   │       ├── CMakeLists.txt
│   │       ├── main.cpp
│   │       ├── resources
│   │       │   ├── app.conf
│   │       │   ├── filetypes
│   │       │   │   └── audio.icon
│   │       │   │       ├── 16x16.png
│   │       │   │       └── 32x32.png
│   │       │   └── icon
│   │       │       ├── 16x16.png
│   │       │       ├── 32x32.png
│   │       │       └── 64x64.png
│   │       ├── ViewerAudioWidget.cpp
│   │       ├── ViewerAudioWidget.h
│   │       ├── ViewerWidget.cpp
│   │       └── ViewerWidget.h
│   ├── CMakeLists.txt
│   ├── coreutils
│   │   ├── benchmark.cpp
│   │   ├── cat.cpp
│   │   ├── chmod.cpp
│   │   ├── chown.cpp
│   │   ├── CMakeLists.txt
│   │   ├── cp.cpp
│   │   ├── date.cpp
│   │   ├── echo.cpp
│   │   ├── fetch.cpp
│   │   ├── free.cpp
│   │   ├── kill.cpp
│   │   ├── ln.cpp
│   │   ├── ls.cpp
│   │   ├── mkdir.cpp
│   │   ├── mv.cpp
│   │   ├── open.cpp
│   │   ├── oshelp.cpp
│   │   ├── ping.cpp
│   │   ├── play.cpp
│   │   ├── profile.cpp
│   │   ├── ps.cpp
│   │   ├── pwd.cpp
│   │   ├── rm.cpp
│   │   ├── rmdir.cpp
│   │   ├── touch.cpp
│   │   ├── truncate.cpp
│   │   ├── uname.cpp
│   │   └── uptime.cpp
│   ├── dsh
│   │   ├── CMakeLists.txt
│   │   ├── Command.cpp
│   │   ├── Command.h
│   │   ├── main.cpp
│   │   ├── Shell.cpp
│   │   ├── Shell.h
│   │   ├── util.cpp
│   │   └── util.h
│   └── nsa-lang
│       ├── CMakeLists.txt
│       ├── include
│       │   ├── nsa_compiler.h
│       │   ├── nsa_lexer.h
│       │   ├── nsa_opcodes.h
│       │   └── nsa_vm.h
│       └── src
│           ├── main.cpp
│           ├── nsa_compiler.cpp
│           ├── nsa_lexer.cpp
│           ├── nsa_vm.cpp
│           └── runtime.cpp
├── README.md
├── scripts
│   ├── base-system.tcl
│   ├── bootlogo.py
│   ├── debugd.py
│   ├── grub.cfg
│   ├── image.tcl
│   ├── kernel-map.tcl
│   ├── nusaos.sh
│   ├── nusaos.tcl
│   ├── qemu.tcl
│   └── version.tcl
├── services
│   ├── CMakeLists.txt
│   ├── dhcpclient
│   │   ├── Client.cpp
│   │   ├── Client.h
│   │   ├── CMakeLists.txt
│   │   ├── DHCP.cpp
│   │   ├── DHCP.h
│   │   └── main.cpp
│   ├── init
│   │   ├── CMakeLists.txt
│   │   ├── main.cpp
│   │   ├── Service.cpp
│   │   └── Service.h
│   ├── pond
│   │   ├── Client.cpp
│   │   ├── Client.h
│   │   ├── CMakeLists.txt
│   │   ├── Display.cpp
│   │   ├── Display.h
│   │   ├── FontManager.cpp
│   │   ├── FontManager.h
│   │   ├── main.cpp
│   │   ├── Mouse.cpp
│   │   ├── Mouse.h
│   │   ├── Server.cpp
│   │   ├── Server.h
│   │   ├── Window.cpp
│   │   └── Window.h
│   └── quack
│       ├── Client.cpp
│       ├── Client.h
│       ├── CMakeLists.txt
│       ├── main.cpp
│       ├── SoundServer.cpp
│       └── SoundServer.h
├── structure.md
├── struktur.md
└── toolchain
    ├── binutils-2.41.patch
    ├── build-ext2-fuse.sh
    ├── build-toolchain.sh
    ├── CMakeToolchain.txt.in
    ├── edit-toolchain.sh
    ├── gcc-13.2.0.patch
    ├── gen-patches.sh
    └── toolchain-common.sh

159 directories, 1079 files
