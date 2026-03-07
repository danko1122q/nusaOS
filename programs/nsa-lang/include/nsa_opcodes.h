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
};