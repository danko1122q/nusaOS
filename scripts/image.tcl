#!/usr/bin/env tclsh

# Source utility functions
set scriptDir [file dirname [info script]]
source [file join $scriptDir "nusaos.tcl"]

# --- Globals ---
set MOUNTED      ""
set dev          ""
set USE_EXISTING ""

# --- Helper: run command, stream output live, exit with message on failure ---
proc run {args} {
    set fd [open "|[list {*}$args] 2>@stderr" r]
    fconfigure $fd -buffering line
    while {[gets $fd line] >= 0} {
        puts $line
        flush stdout
    }
    if {[catch {close $fd} err opts]} {
        set ec [lindex [dict get $opts -errorcode] 0]
        if {$ec eq "CHILDSTATUS"} {
            set code [lindex [dict get $opts -errorcode] 2]
            if {$code != 0} {
                global ON_FAIL
                puts stderr "\x1b\[31m\[ERROR\] Command failed: [join $args { }]\x1b\[39m"
                if {[info exists ON_FAIL] && $ON_FAIL ne ""} {
                    catch {{*}$ON_FAIL}
                }
                exit $code
            }
        }
    }
}

# --- Helper: run command, capture stdout, exit on failure ---
proc run_capture {args} {
    if {[catch {exec {*}$args} result]} {
        global ON_FAIL
        puts stderr "\x1b\[31m\[ERROR\] Command failed: [join $args { }]: $result\x1b\[39m"
        if {[info exists ON_FAIL] && $ON_FAIL ne ""} {
            catch {{*}$ON_FAIL}
        }
        exit 1
    }
    return $result
}

# --- Cleanup proc ---
proc detach_and_unmount {} {
    global MOUNTED dev SYSTEM

    msg "Cleaning up..."
    catch {exec sync}

    if {$MOUNTED ne ""} {
        catch {exec umount -l mnt/}
        after 500
        if {[file isdirectory mnt]} {
            catch {file delete mnt}
        }
        set MOUNTED ""
    }

    if {$dev ne ""} {
        if {$SYSTEM eq "Darwin"} {
            catch {exec hdiutil detach $dev}
        } else {
            catch {exec sync}
            catch {exec losetup -d $dev}
        }
        set dev ""
    }
}

set ON_FAIL detach_and_unmount

# Trap any unexpected error at top level so cleanup always runs
proc bgerror {msg} {
    global ON_FAIL
    puts stderr "\x1b\[31m\[ERROR\] Unexpected error: $msg\x1b\[39m"
    if {[info exists ON_FAIL] && $ON_FAIL ne ""} {
        catch {{*}$ON_FAIL}
    }
    exit 1
}

# --- Privilege check ---
if {[string trim [exec id -u]] ne "0"} {
    set tclsh [info nameofexecutable]
    run sudo -E -- $tclsh [info script] {*}$argv
    exit 0
}

if {![info exists ::env(SUDO_UID)]} { set ::env(SUDO_UID) 0 }
if {![info exists ::env(SUDO_GID)]} { set ::env(SUDO_GID) 0 }

# --- Env setup ---
set SYSTEM          [string trim [exec uname -s]]
set DU_COMMAND      du
set IMAGE_NAME      nusaOS.img
set IMAGE_EXTRASIZE 200000
set SOURCE_DIR      $::env(SOURCE_DIR)

if {$SYSTEM eq "Darwin"} {
    set extraPaths "/usr/local/opt/e2fsprogs/bin:/usr/local/opt/e2fsprogs/sbin:/opt/homebrew/opt/e2fsprogs/bin:/opt/homebrew/opt/e2fsprogs/sbin"
    set ::env(PATH) "$extraPaths:$::env(PATH)"
    set DU_COMMAND gdu
}

# --- Image creation or use of provided device ---
if {$argc == 0} {
    set USER_SIZE 0
    set userDir [file join $SOURCE_DIR user]
    if {[file isdirectory $userDir]} {
        set USER_SIZE [lindex [split [exec $DU_COMMAND -sk $userDir] \t] 0]
    }

    set rootSize [lindex [split [exec $DU_COMMAND -sk root] \t] 0]
    set IMAGE_SIZE [expr {($rootSize + $IMAGE_EXTRASIZE + $USER_SIZE + 1023) / 1024 * 1024}]

    if {[file exists $IMAGE_NAME]} {
        set USE_EXISTING 1
        msg "Using existing image..."
    } else {
        msg "Creating image ($IMAGE_SIZE K)..."
        run qemu-img create -q -f raw $IMAGE_NAME ${IMAGE_SIZE}k
        catch {exec chown $::env(SUDO_UID):$::env(SUDO_GID) $IMAGE_NAME}
    }

    if {$SYSTEM eq "Darwin"} {
        msg "Attaching image..."
        set result [run_capture hdiutil attach -nomount $IMAGE_NAME]
        set dev [lindex [split $result] 0]
        set part "s1"
        if {$dev eq ""} {
            err "hdiutil attach returned empty device"
            exit 1
        }
    } else {
        msg "Making loopback device..."
        if {[catch {exec losetup --find --partscan --show $IMAGE_NAME} result]} {
            err "losetup failed: $result"
            exit 1
        }
        set dev [string trim $result]
        set part "p1"
        if {$dev eq ""} {
            err "losetup returned empty device"
            exit 1
        }
        msg "Loop device: $dev"
    }
} else {
    set dev  [lindex $argv 0]
    set part "1"
}

# --- Partition and format ---
if {$USE_EXISTING eq ""} {
    if {$SYSTEM eq "Darwin"} {
        run diskutil partitionDisk [file tail $dev] 1 MBR fuse-ext2 nusaOS 100%
    } else {
        run parted -s $dev mklabel msdos mkpart primary ext2 32k 100% -a minimal set 1 boot on
    }
    # mke2fs via sh because of 'yes |' pipe
    # Ignore non-zero exit from mke2fs as it emits warnings to stderr even on success
    catch {exec sh -c "yes | mke2fs -q -I 128 -b 1024 ${dev}${part} 2>&1"} e
    msg "mke2fs: $e"
}

# --- Mount ---
file mkdir mnt
if {$SYSTEM eq "Darwin"} {
    run fuse-ext2 ${dev}${part} mnt -o rw+,allow_other,uid=$::env(SUDO_UID),gid=$::env(SUDO_GID)
} else {
    run mount ${dev}${part} mnt/
}
set MOUNTED 1

# --- GRUB install (Linux only, new images only) ---
if {$USE_EXISTING eq "" && $SYSTEM ne "Darwin"} {
    set GRUB_COMMAND grub-install
    if {[catch {exec which grub-install}]} {
        set GRUB_COMMAND grub2-install
    }

    # --modules must be a single quoted argument
    run $GRUB_COMMAND \
        --boot-directory=mnt/boot \
        --target=i386-pc \
        "--locales=" \
        --no-floppy \
        "--modules=ext2 part_msdos" \
        $dev --force

    set grubCfgSrc [file join $SOURCE_DIR scripts grub.cfg]
    if {[file isdirectory mnt/boot/grub2]} {
        file copy -force $grubCfgSrc mnt/boot/grub2/grub.cfg
    } elseif {[file isdirectory mnt/boot/grub]} {
        file copy -force $grubCfgSrc mnt/boot/grub/grub.cfg
    }
}

# --- Populate filesystem ---
msg "Running base-system..."
run [info nameofexecutable] [file join $SOURCE_DIR scripts base-system.tcl] mnt/

catch {exec sync}

if {$argc == 0} {
    detach_and_unmount
    success "Done! Saved to $IMAGE_NAME."
} else {
    success "Done!"
}