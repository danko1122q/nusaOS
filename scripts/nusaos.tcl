#!/usr/bin/env tclsh

proc err {msg} {
    puts "\x1b\[31m\[ERROR\] $msg \x1b\[39m"
}

proc msg {message} {
    puts "\x1b\[36m\[INFO\] $message \x1b\[39m"
}

proc warn {message} {
    puts "\x1b\[33m\[WARN\] $message \x1b\[39m"
}

proc success {message} {
    puts "\x1b\[32m\[SUCCESS\] $message \x1b\[39m"
}

proc fail {message} {
    global FAILING ON_FAIL

    err $message

    if {![info exists FAILING] || $FAILING eq ""} {
        set FAILING 1
        if {[info exists ON_FAIL] && $ON_FAIL ne ""} {
            {*}$ON_FAIL
        }
    }

    exit 1
}