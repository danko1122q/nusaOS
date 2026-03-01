#!/usr/bin/env tclsh

set SCRIPTDIR [file dirname [file normalize [info script]]]
source [file join $SCRIPTDIR nusaos.tcl]

# --- Baca variabel dari environment ---
set ARCH        [expr {[info exists ::env(ARCH)]        ? $::env(ARCH)        : "i686"}]
set KERNEL_NAME [expr {[info exists ::env(KERNEL_NAME)] ? $::env(KERNEL_NAME) : "nusak32"}]

# --- KVM check ---
set USE_KVM "0"
if {[info exists ::env(USE_KVM)]} {
    set USE_KVM $::env(USE_KVM)
} else {
    if {[file exists /dev/kvm] && [file readable /dev/kvm] && [file writable /dev/kvm]} {
        set HOST_ARCH [string trim [exec uname -m]]
        if {$ARCH eq $HOST_ARCH || ($ARCH eq "i686" && $HOST_ARCH eq "x86_64")} {
            set USE_KVM "1"
        } else {
            warn "Host architecture ($HOST_ARCH) does not match guest architecture ($ARCH) - not using kvm."
        }
    }
}

# --- Image ---
set NUSAOS_IMAGE [expr {[info exists ::env(NUSAOS_IMAGE)] ? $::env(NUSAOS_IMAGE) : "nusaOS.img"}]

# --- Akselerasi QEMU ---
set NUSAOS_QEMU_ACCEL ""
if {[info exists ::env(NUSAOS_QEMU_ACCEL)]} {
    set NUSAOS_QEMU_ACCEL $::env(NUSAOS_QEMU_ACCEL)
} else {
    if {[auto_execok wslpath] ne ""} {
        set NUSAOS_QEMU_ACCEL "-accel whpx,kernel-irqchip=off -accel tcg"
    } elseif {$USE_KVM ne "0"} {
        set NUSAOS_QEMU_ACCEL "-enable-kvm"
    }
}

# --- Arsitektur ---
set NUSAOS_QEMU_MACHINE ""
switch -- $ARCH {
    i686 {
        set QEMU_SYSTEM       "i386"
        set NUSAOS_QEMU_MEM   "512M"
        set NUSAOS_QEMU_DRIVE [list -drive "file=$NUSAOS_IMAGE,format=raw,index=0,media=disk"]
        set NUSAOS_QEMU_DEVICES [list -device ac97]
        set NUSAOS_QEMU_SERIAL  [list -serial stdio]
    }
    aarch64 {
        set QEMU_SYSTEM         "aarch64"
        set NUSAOS_QEMU_MACHINE [list -machine raspi3b]
        set NUSAOS_QEMU_MEM     "1G"
        set NUSAOS_QEMU_DRIVE   ""
        set NUSAOS_QEMU_DEVICES ""
        set NUSAOS_QEMU_SERIAL  [list -serial null -serial stdio]
    }
    default {
        fail "Unsupported architecture $ARCH."
    }
}

# --- Cari binary QEMU ---
if {[auto_execok wslpath] ne ""} {
    set WIN_QEMU_DIR [expr {[info exists ::env(NUSAOS_WIN_QEMU_INSTALL_DIR)]
        ? $::env(NUSAOS_WIN_QEMU_INSTALL_DIR)
        : "C:\\Program Files\\qemu"}]
    set NUSAOS_QEMU "[string trim [exec wslpath $WIN_QEMU_DIR]]/qemu-system-$QEMU_SYSTEM.exe"
} else {
    set NUSAOS_QEMU "qemu-system-$QEMU_SYSTEM"
}

# --- Deteksi display ---
set NUSAOS_QEMU_DISPLAY ""
if {[info exists ::env(NUSAOS_QEMU_DISPLAY)]} {
    set NUSAOS_QEMU_DISPLAY $::env(NUSAOS_QEMU_DISPLAY)
} else {
    catch {exec $NUSAOS_QEMU --display help} help_out
    if {[regexp -nocase {sdl} $help_out] && $ARCH ne "aarch64"} {
        set NUSAOS_QEMU_DISPLAY [list -display sdl]
    } elseif {[regexp -nocase {cocoa} $help_out]} {
        set NUSAOS_QEMU_DISPLAY [list -display cocoa]
    }
}

# --- Kernel args ---
set NUSAOS_KERNEL_ARGS [expr {[info exists ::env(NUSAOS_KERNEL_ARGS)]
    ? $::env(NUSAOS_KERNEL_ARGS)
    : "init=/bin/dsh [join $argv { }]"}]

# --- Susun command QEMU ---
set CMD [list $NUSAOS_QEMU \
    -s \
    -kernel "kernel/${KERNEL_NAME}" \
    -m $NUSAOS_QEMU_MEM]

foreach part [list \
    $NUSAOS_QEMU_SERIAL   \
    $NUSAOS_QEMU_DEVICES  \
    $NUSAOS_QEMU_DRIVE    \
    $NUSAOS_QEMU_MACHINE  \
    $NUSAOS_QEMU_DISPLAY  \
] {
    if {$part ne ""} { lappend CMD {*}$part }
}

if {$NUSAOS_QEMU_ACCEL ne ""} {
    lappend CMD {*}[split $NUSAOS_QEMU_ACCEL]
}

lappend CMD -append $NUSAOS_KERNEL_ARGS

exec {*}$CMD >@stdout 2>@stderr