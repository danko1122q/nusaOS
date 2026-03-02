#!/usr/bin/env tclsh

if {$argc < 1} {
    puts stderr "Usage: version.tcl <output_file>"
    exit 1
}

set OUTPUT [lindex $argv 0]

set VERSION_MAJOR "0"
set VERSION_MINOR "4"
set VERSION_PATCH "0"
set GIT_REVISION  [string trim [exec git rev-parse --short HEAD]]
set DATE          [string trim [exec date]]

set fd [open $OUTPUT w]
puts $fd "/* This file was generated automatically */"
puts $fd "#pragma once"
puts $fd ""
puts $fd "const int NUSAOS_VERSION_MAJOR = ${VERSION_MAJOR};"
puts $fd "const int NUSAOS_VERSION_MINOR = ${VERSION_MINOR};"
puts $fd "const int NUSAOS_VERSION_PATCH = ${VERSION_PATCH};"
puts $fd "const char* NUSAOS_VERSION_STRING = \"${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}\";"
puts $fd "const char* NUSAOS_REVISION = \"${GIT_REVISION}\";"
close $fd