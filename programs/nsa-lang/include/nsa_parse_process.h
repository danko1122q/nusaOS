/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 danko1122q / nusaOS project */

/*
 * nsa_parse_process.h — Process/OS primitive parsers (v2.5.2)
 *
 * Handles: fork, exec, waitpid, exit, getpid, sleep, getenv, peek, poke
 *
 * Included directly by nsa_compiler.cpp (not a separate translation unit)
 * because it needs access to the CS struct and helper macros.
 * Place this #include inside the NsaCompiler namespace block.
 */

#pragma once

/* ── fork ─────────────────────────────────────────────────────────────
 *   fork dst
 *   fork(dst)           ← () style also works
 */
static void parse_fork(CS& cs, const Toks& t) {
    if (t.size() < 2) { cs.error("expected: fork <dst_int>"); return; }
    uint8_t dst; if (!cs.intern(t[1].text, SYM_INT, dst)) return;
    cs.emit(OP_FORK);
    cs.emit(dst);
}

/* ── exec ─────────────────────────────────────────────────────────────
 *   exec dst path arg0 [arg1 ... argN]
 *
 *   path  — string var with executable path
 *   arg0  — argv[0], conventionally the program name
 *   argN  — additional arguments (up to 15 total, max 16 including arg0)
 *
 *   Encoding: OP_EXEC  dst  path_id  argc(u8)  arg0_id ... argN_id
 *
 *   Example:
 *     let prog = "/bin/echo"
 *     let a0   = "echo"
 *     let a1   = "Hello from NSA!"
 *     exec ret prog a0 a1
 */
static void parse_exec(CS& cs, const Toks& t) {
    if (t.size() < 4) {
        cs.error("expected: exec <dst> <path_str> <arg0> [args...]");
        return;
    }
    uint8_t dst; if (!cs.intern(t[1].text, SYM_INT, dst)) return;

    uint8_t path_id; SymType pt;
    if (!cs.lookup(t[2].text, path_id, &pt)) return;
    if (pt != SYM_STR) { cs.error("exec: path must be a string variable"); return; }

    size_t argc = t.size() - 3;
    if (argc > 16) { cs.error("exec: max 16 arguments"); return; }

    cs.emit(OP_EXEC);
    cs.emit(dst);
    cs.emit(path_id);
    cs.emit((uint8_t)argc);
    for (size_t i = 0; i < argc; i++) {
        uint8_t arg_id; SymType at;
        if (!cs.lookup(t[3+i].text, arg_id, &at)) return;
        if (at != SYM_STR) { cs.error("exec: argument must be a string variable"); return; }
        cs.emit(arg_id);
    }
}

/* ── waitpid ──────────────────────────────────────────────────────────
 *   waitpid dst pid [opts]
 *   opts defaults to blocking (0). WNOHANG=1, WUNTRACED=2.
 *   dst = child pid returned, or -errno on error.
 */
static void parse_waitpid(CS& cs, const Toks& t) {
    if (t.size() < 3) {
        cs.error("expected: waitpid <dst_int> <pid_var> [opts]");
        return;
    }
    uint8_t dst; if (!cs.intern(t[1].text, SYM_INT, dst)) return;
    uint8_t pid_id; if (!cs.lookup(t[2].text, pid_id)) return;
    cs.emit(OP_WAITPID);
    cs.emit(dst);
    cs.emit(pid_id);
    if (t.size() >= 4 && t[3].kind == TK::TK_INT) {
        cs.emit(0x01); cs.emit_i32(t[3].ival);
    } else if (t.size() >= 4) {
        uint8_t ov; if (!cs.lookup(t[3].text, ov)) return;
        cs.emit(0x00); cs.emit(ov);
    } else {
        cs.emit(0x02); /* default: block until done */
    }
}

/* ── exit ─────────────────────────────────────────────────────────────
 *   exit [code]
 *   Calls SYS_EXIT. Never returns. If code omitted, exits with 0.
 */
static void parse_exit(CS& cs, const Toks& t) {
    cs.emit(OP_EXIT);
    if (t.size() >= 2 && t[1].kind == TK::TK_INT) {
        cs.emit(0x01); cs.emit_i32(t[1].ival);
    } else if (t.size() >= 2) {
        uint8_t id; if (!cs.lookup(t[1].text, id)) return;
        cs.emit(0x00); cs.emit(id);
    } else {
        cs.emit(0x02); /* exit(0) */
    }
}

/* ── getpid (v2.5.2) ──────────────────────────────────────────────────
 *   getpid dst
 *   Stores current process ID in dst.
 */
static void parse_getpid(CS& cs, const Toks& t) {
    if (t.size() < 2) { cs.error("expected: getpid <dst_int>"); return; }
    uint8_t dst; if (!cs.intern(t[1].text, SYM_INT, dst)) return;
    cs.emit(OP_GETPID);
    cs.emit(dst);
}

/* ── sleep (v2.5.2) ───────────────────────────────────────────────────
 *   sleep ms
 *   Sleep for ms milliseconds. Accepts a variable or integer literal.
 *
 *   Example:
 *     sleep 1000       // sleep 1 second
 *     let t = 500
 *     sleep t          // sleep 0.5 seconds
 */
static void parse_sleep(CS& cs, const Toks& t) {
    if (t.size() < 2) { cs.error("expected: sleep <ms>"); return; }
    cs.emit(OP_SLEEP);
    if (t[1].kind == TK::TK_INT) {
        cs.emit(0x01); cs.emit_i32(t[1].ival);
    } else {
        uint8_t id; if (!cs.lookup(t[1].text, id)) return;
        cs.emit(0x00); cs.emit(id);
    }
}

/* ── getenv (v2.5.2) ──────────────────────────────────────────────────
 *   getenv dst name
 *   Reads environment variable <name> into dst (string).
 *   If not set, dst = "".
 *
 *   Example:
 *     let val = ""
 *     let key = "HOME"
 *     getenv val key
 *     print val
 */
static void parse_getenv(CS& cs, const Toks& t) {
    if (t.size() < 3) { cs.error("expected: getenv <dst_str> <name_str>"); return; }
    uint8_t dst; if (!cs.intern(t[1].text, SYM_STR, dst)) return;
    uint8_t name_id; SymType nt;
    if (!cs.lookup(t[2].text, name_id, &nt)) return;
    if (nt != SYM_STR) { cs.error("getenv: name must be a string variable"); return; }
    cs.emit(OP_GETENV);
    cs.emit(dst);
    cs.emit(name_id);
}

/* ── peek / poke (v2.5.2) ─────────────────────────────────────────────
 *   peek dst addr       — read int32 from memory address
 *   poke addr src       — write int32 to memory address
 *   peek8 dst addr      — read uint8 from memory address
 *   poke8 addr src      — write byte to memory address
 *
 *   These are low-level memory access primitives.
 *   addr must be an int variable holding a valid memory address.
 *   Typically used with addrof or sysbuf to get the address first.
 *
 *   Example:
 *     let buf = 0
 *     let sz  = 16
 *     sysbuf buf sz         // allocate buffer, buf = address
 *     let val = 0x41414141
 *     poke buf val          // write 0x41414141 to buffer
 *     let out = 0
 *     peek out buf          // read back -> out = 0x41414141
 */
static void parse_peek(CS& cs, const Toks& t, NsaOpcode op) {
    if (t.size() < 3) { cs.error("expected: peek <dst_int> <addr_int>"); return; }
    uint8_t dst; if (!cs.intern(t[1].text, SYM_INT, dst)) return;
    uint8_t addr_id; if (!cs.lookup(t[2].text, addr_id)) return;
    cs.emit(op);
    cs.emit(dst);
    cs.emit(addr_id);
}

static void parse_poke(CS& cs, const Toks& t, NsaOpcode op) {
    if (t.size() < 3) { cs.error("expected: poke <addr_int> <src_int>"); return; }
    uint8_t addr_id; if (!cs.lookup(t[1].text, addr_id)) return;
    uint8_t src_id;  if (!cs.lookup(t[2].text, src_id))  return;
    cs.emit(op);
    cs.emit(addr_id);
    cs.emit(src_id);
}
