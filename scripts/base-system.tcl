#!/usr/bin/env tclsh

# Source utility functions
set scriptDir [file dirname [info script]]
source [file join $scriptDir "nusaos.tcl"]

# --- Argument check ---
if {$argc < 1} {
    fail "Please provide a directory to create the filesystem in."
}

set FS_DIR [lindex $argv 0]
set SOURCE_DIR $::env(SOURCE_DIR)
set ARCH      $::env(ARCH)

# Helper: run a command and exit on failure
proc run {args} {
    if {[catch {exec {*}$args >@stdout 2>@stderr} err]} {
        return -code error $err
    }
}

# Helper: safe rsync wrapper
proc rsync_or_fail {src dst label} {
    if {[catch {exec rsync -auH --inplace {*}$src $dst >@stdout 2>@stderr} e]} {
        puts "Couldn't copy $label."
        exit 1
    }
}

# --- Copy base, kernel, and user files ---
msg "Copying base and kernel to filesystem..."

set userDir [file join $SOURCE_DIR user]
if {[file isdirectory $userDir]} {
    rsync_or_fail [list {*}[glob -nocomplain $userDir/*]] $FS_DIR "user folder"
} else {
    warn "No user folder, or empty user folder."
}

rsync_or_fail [list {*}[glob -nocomplain [file join $SOURCE_DIR base]/*]] $FS_DIR "base"
rsync_or_fail [list {*}[glob -nocomplain root/*]] $FS_DIR/ "root"

# --- Copy toolchain libs and headers ---
msg "Copying toolchain libs and headers..."

set tcRoot [file join $SOURCE_DIR toolchain tools $ARCH ${ARCH}-pc-nusaos]
rsync_or_fail \
    [list -aH --update -t -r {*}[glob -nocomplain [file join $tcRoot lib]/*]] \
    [file join $FS_DIR lib] "libs"
rsync_or_fail \
    [list -aH --update -t -r {*}[glob -nocomplain [file join $tcRoot include]/*]] \
    [file join $FS_DIR usr include] "headers"

# --- Root filesystem setup ---
msg "Setting up root filesystem..."
msg "Setting up devices..."

set devDir [file join $FS_DIR dev]
if {[file isdirectory $devDir]} {
    foreach f [glob -nocomplain $devDir/*] {
        file delete -force $f
    }
}
file mkdir $devDir

proc mknod {path type major minor} {
    exec mknod $path $type $major $minor
}

mknod [file join $devDir tty0]   c 4  0
mknod [file join $devDir hda]    b 3  0
mknod [file join $devDir random] c 1  8
mknod [file join $devDir null]   c 1  3
mknod [file join $devDir zero]   c 1  5
mknod [file join $devDir klog]   c 1  16
mknod [file join $devDir fb0]    b 29 0

file mkdir [file join $devDir input]
mknod [file join $devDir input keyboard] c 13 0
mknod [file join $devDir input mouse]    c 13 1
mknod [file join $devDir ptmx]           c 5  2
mknod [file join $devDir snd0]           c 69 2

file mkdir [file join $devDir pts]

msg "Setting up directories..."
exec chmod -R g+rX,o+rX $FS_DIR

msg "Setting up /proc/..."
file mkdir [file join $FS_DIR proc]
exec chmod 555 [file join $FS_DIR proc]

msg "Setting up /sock/..."
file mkdir [file join $FS_DIR sock]
exec chmod 777 [file join $FS_DIR sock]

msg "Setting up /etc/..."
exec chown -R 0:0 [file join $FS_DIR etc]

success "Done setting up root filesystem!"