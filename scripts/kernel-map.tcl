#!/usr/bin/env tclsh

if {$argc < 1} {
    puts stderr "Usage: kernel-map.tcl <binary>"
    exit 1
}

set binary [lindex $argv 0]

set fd [open "kernel.map" w]
set output [exec nm -C -n $binary]
puts $fd $output
close $fd