/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 danko1122q / nusaOS project */

#pragma once
#include <stdint.h>
#include <stddef.h>

/* ── Magic & limits ───────────────────────────────────────────────────── */
static const char    NSA_MAGIC[6]       = {'\x7f', 'N', 'S', 'A', 0x02, 0x00};
/* NSS module magic — same version byte so the toolchain can validate it  */
static const char    NSS_MAGIC[6]       = {'\x7f', 'N', 'S', 'S', 0x01, 0x00};
static const uint8_t NSA_MAX_VARS       = 200;
static const size_t  NSA_MAX_STR_LEN    = 254;
static const uint8_t NSA_MAX_LOCALS     = 64;
static const uint8_t NSA_MAX_CALL_DEPTH = 64;
/* Arrays: each array occupies one var slot (the base-id).
   Elements are stored in the NEXT (size) consecutive slots.
   Max 64 elements per array, max 32 arrays per program.       */
static const uint8_t NSA_MAX_ARRAY_SIZE = 64;
static const char    NSA_EXT[]          = ".nsa";
static const char    NSA_BIN_EXT[]      = ".nbin";
static const char    NSS_EXT[]          = ".nss";

/* ── Opcode table ─────────────────────────────────────────────────────── */
enum NsaOpcode : uint8_t {
    /* Control */
    OP_NOP          = 0x00,
    OP_HALT         = 0xFF,

    /* I/O */
    OP_PRINT_STR    = 0x01,
    OP_PRINT_VAR    = 0x02,
    OP_PRINT_STR_NL = 0x03,
    OP_PRINT_VAR_NL = 0x04,
    OP_INPUT_INT    = 0x05,
    OP_INPUT_STR    = 0x06,

    /* Variables */
    OP_SET_STR      = 0x10,
    OP_SET_INT      = 0x11,
    OP_SET_BOOL     = 0x12,
    OP_COPY         = 0x13,

    /* Integer arithmetic (immediate) */
    OP_ADD_IMM      = 0x20,
    OP_SUB_IMM      = 0x21,
    OP_MUL_IMM      = 0x22,
    OP_DIV_IMM      = 0x23,
    OP_MOD_IMM      = 0x24,

    /* Integer arithmetic (var) */
    OP_ADD_VAR      = 0x28,
    OP_SUB_VAR      = 0x29,
    OP_MUL_VAR      = 0x2A,
    OP_DIV_VAR      = 0x2B,
    OP_MOD_VAR      = 0x2C,

    /* Unary */
    OP_INC          = 0x30,
    OP_DEC          = 0x31,
    OP_NOT          = 0x32,
    OP_NEG          = 0x33,

    /* String ops */
    OP_CONCAT       = 0x40,
    OP_CONCAT_LIT   = 0x41,
    OP_LEN          = 0x42,
    OP_STR_TO_INT   = 0x43,
    OP_INT_TO_STR   = 0x44,

    /* ── String comparison ───────────────────────────────────────────────
     * OP_SCMP_EQ/NE  dst_bool  str_a  str_b
     * Compare two string variables with strcmp, store bool result.     */
    OP_SCMP_EQ      = 0x45,
    OP_SCMP_NE      = 0x46,

    /* Comparison → bool (integer) */
    OP_CMP_EQ       = 0x50,
    OP_CMP_NE       = 0x51,
    OP_CMP_LT       = 0x52,
    OP_CMP_GT       = 0x53,
    OP_CMP_LE       = 0x54,
    OP_CMP_GE       = 0x55,

    /* Logical */
    OP_AND          = 0x58,
    OP_OR           = 0x59,

    /* Forward jumps */
    OP_JMP_FWD      = 0x60,
    OP_JMP_IF_TRUE  = 0x61,
    OP_JMP_IF_FALSE = 0x62,
    OP_JMP_IF_EQ    = 0x63,
    OP_JMP_IF_NE    = 0x64,
    OP_JMP_IF_LT    = 0x65,
    OP_JMP_IF_GT    = 0x66,
    OP_JMP_IF_LE    = 0x67,
    OP_JMP_IF_GE    = 0x68,

    /* Backward jumps */
    OP_JMP_BACK         = 0x70,
    OP_JMP_BACK_TRUE    = 0x71,
    OP_JMP_BACK_FALSE   = 0x72,
    OP_JMP_BACK_NZ      = 0x73,
    OP_JMP_BACK_Z       = 0x74,

    /* Functions */
    OP_CALL         = 0x80,
    OP_RET          = 0x81,
    OP_LOAD_ARG     = 0x82,
    OP_STORE_RET    = 0x83,

    /* ── Arrays ──────────────────────────────────────────────────────────
     *
     * Layout in var table:
     *   var[base_id]        = array descriptor: ival = element count
     *   var[base_id+1]      = element 0
     *   ...
     *   var[base_id+size]   = element size-1
     *
     * OP_ARR_GET  dst_var  base_id  idx_var
     * OP_ARR_SET  base_id  idx_var  src_var
     * OP_ARR_SET_IMM  base_id  idx_var  type  value...
     *   type byte: 0x01=int(i32), 0x02=str(len+bytes), 0x03=bool(u8)
     * OP_ARR_LEN  dst_int  base_id
     * ------------------------------------------------------------------ */
    OP_ARR_GET      = 0x90,
    OP_ARR_SET      = 0x91,
    OP_ARR_SET_IMM  = 0x92,
    OP_ARR_LEN      = 0x93,

    /* ── Global variables (NSS modules) ──────────────────────────────────
     * These opcodes access the global var table directly even inside
     * a function, enabling module-level global variables that persist.
     *
     * OP_GSET_INT   gid  i32     — set global[gid] = int literal
     * OP_GSET_STR   gid  len+bytes
     * OP_GSET_BOOL  gid  u8
     * OP_GCOPY      dst_gid  src_gid   — copy between globals
     * OP_GLOAD      local_id  gid      — load global → current local/global
     * OP_GSTORE     gid  local_id      — store local/global → global
     * ------------------------------------------------------------------ */
    OP_GSET_INT     = 0xA0,
    OP_GSET_STR     = 0xA1,
    OP_GSET_BOOL    = 0xA2,
    OP_GCOPY        = 0xA3,
    OP_GLOAD        = 0xA4,
    OP_GSTORE       = 0xA5,

    /* ── String indexing (v2.5) ──────────────────────────────────────────
     * OP_SGET  dst_str  src_str  idx_var   — get char at index → 1-char string
     * OP_SSET  dst_str  idx_var  src_str   — set char at index from 1-char string
     * OP_SSUB  dst_str  src_str  start_var  len_var  — substring
     * ------------------------------------------------------------------ */
    OP_SGET         = 0xB0,
    OP_SSET         = 0xB1,
    OP_SSUB         = 0xB2,

    /* ── File I/O (v2.5) ─────────────────────────────────────────────────
     * OP_FOPEN   fd_var  path_var  mode_var
     *   Opens a file. mode_var must be a string: "r", "w", or "a".
     *   Stores an integer file descriptor in fd_var (negative = error).
     *
     * OP_FCLOSE  fd_var
     *   Closes the file descriptor stored in fd_var.
     *
     * OP_FREAD   dst_str  fd_var
     *   Reads the entire remaining file content into dst_str.
     *
     * OP_FWRITE  fd_var  src_str
     *   Writes the string stored in src_str to the file.
     *
     * OP_FEXISTS dst_bool  path_var
     *   Sets dst_bool = true if the path exists, false otherwise.
     * ------------------------------------------------------------------ */
    OP_FOPEN        = 0xB8,
    OP_FCLOSE       = 0xB9,
    OP_FREAD        = 0xBA,
    OP_FWRITE       = 0xBB,
    OP_FEXISTS      = 0xBC,

    /* ── Floating point (v2.5) ───────────────────────────────────────────
     * Variables declared with 'let x = 3.14' get type SYM_FLOAT.
     * Float vars store their value in the dval field of VarSlot.
     *
     * OP_SET_FLOAT  dst  f64(8 bytes LE)  — load float literal
     * OP_FADD       dst  a  b
     * OP_FSUB       dst  a  b
     * OP_FMUL       dst  a  b
     * OP_FDIV       dst  a  b
     * OP_FNEG       dst               — negate float
     * OP_ITOF       dst  src_int      — int → float
     * OP_FTOI       dst  src_float    — float → int (truncate)
     * OP_FTOS       dst  src_float    — float → str  (6 decimal places)
     * OP_FCMP       dst_bool  a  op_byte  b
     *   op_byte: 0=EQ 1=NE 2=LT 3=GT 4=LE 5=GE
     * ------------------------------------------------------------------ */
    OP_SET_FLOAT    = 0xC0,
    OP_FADD         = 0xC1,
    OP_FSUB         = 0xC2,
    OP_FMUL         = 0xC3,
    OP_FDIV         = 0xC4,
    OP_FNEG         = 0xC5,
    OP_ITOF         = 0xC6,
    OP_FTOI         = 0xC7,
    OP_FTOS         = 0xC8,
    OP_FCMP         = 0xC9,
    OP_FPRINT       = 0xCA,
    OP_FPRINT_NL    = 0xCB,

    /* ── Syscall interface (v2.5) ────────────────────────────────────────
     *
     * OP_SYSCALL  dst_int  num_var_or_imm  a  b  c
     *   Calls int 0x80 on i386 nusaOS.
     *   eax = syscall number (from var or immediate)
     *   ebx = arg1 (var), ecx = arg2 (var), edx = arg3 (var)
     *   Result stored in dst_int (signed).
     *
     *   Encoding: opcode  dst  flags  num  a  b  c
     *     flags byte: bit0 = num is immediate i32 (4 extra bytes follow)
     *                 otherwise num is a var id (1 byte)
     *
     * OP_SYSBUF_ALLOC  buf_var  size_int
     *   Allocates a fixed buffer in VM heap for passing pointers to syscalls.
     *   Returns the integer address of the buffer in buf_var (SYM_INT).
     *
     * OP_SYSBUF_WRITE  buf_var  offset_int  src_str
     *   Writes string into buffer at offset.
     *
     * OP_SYSBUF_READ   dst_str  buf_var  offset_int  len_int
     *   Reads bytes from buffer into string variable.
     *
     * OP_ADDR_OF  dst_int  src_var
     *   Stores the address of src_var's sval (string buffer) in dst_int.
     *   Useful for passing string pointers directly to syscalls.
     *
     * ------------------------------------------------------------------ */
    OP_SYSCALL      = 0xD0,
    OP_SYSBUF_ALLOC = 0xD1,
    OP_SYSBUF_WRITE = 0xD2,
    OP_SYSBUF_READ  = 0xD3,
    OP_ADDR_OF      = 0xD4,

    /* ── fork / exec / waitpid (v2.5.1) ─────────────────────────────────
     *
     * OP_FORK  dst_int
     *   Calls SYS_FORK. In parent: dst = child PID (>0).
     *   In child:  dst = 0.
     *   On error:  dst = -1.
     *
     * OP_EXEC  dst_int  path_str  arg0_str [arg1_str ... argN_str]
     *   Encoding: opcode  dst  path_id  argc(u8)  [arg_id ...]
     *   Builds a char* argv[] array on the stack, then calls SYS_EXECVE.
     *   argv[0] = arg0, ..., argv[argc-1] = argN, argv[argc] = NULL.
     *   envp = NULL (inherit from parent via kernel default).
     *   On success: never returns (child process replaced).
     *   On failure: dst = -errno.
     *   Max 16 arguments.
     *
     * OP_WAITPID  dst_int  pid_var  opts_var_or_imm
     *   Calls SYS_WAITPID(pid, &status, opts).
     *   dst = return value (child pid or -errno).
     *   Status is discarded (use OP_SYSCALL directly if you need it).
     *
     * OP_EXIT  code_var_or_imm
     *   Calls SYS_EXIT. Never returns.
     * ------------------------------------------------------------------ */
    OP_FORK         = 0xD5,
    OP_EXEC         = 0xD6,
    OP_WAITPID      = 0xD7,
    OP_EXIT         = 0xD8,
};
