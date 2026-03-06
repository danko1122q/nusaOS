/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 danko1122q / nusaOS project */

#pragma once
#include <stdint.h>
#include <stddef.h>

/* ── Magic & limits ───────────────────────────────────────────────────── */
static const char    NSA_MAGIC[6]       = {'\x7f', 'N', 'S', 'A', 0x02, 0x00};
static const uint8_t NSA_MAX_VARS       = 200;
static const size_t  NSA_MAX_STR_LEN    = 254;
static const uint8_t NSA_MAX_LOCALS     = 64;    /* local slots per function frame */
static const uint8_t NSA_MAX_CALL_DEPTH = 64;    /* maximum nested call depth      */
static const char    NSA_EXT[]          = ".nsa";
static const char    NSA_BIN_EXT[]      = ".nbin";

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

    /* Comparison → bool */
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

    /* ── Functions (new in v2.1) ─────────────────────────────────────────
     *
     * Bytecode layout around a call:
     *
     *   [LOAD_ARG local_id global_id]  ...  (one per argument)
     *   [CALL u16:abs_addr]
     *   [STORE_RET global_id local_id]      (only if return value requested)
     *
     * Inside a function body, variable IDs refer to LOCAL slots (0..63).
     * Globals are never touched directly from inside a function.
     *
     * CALL  pushes {ret_ip} onto the call stack and sets ip = abs_addr.
     * RET   pops the call stack and restores ip.
     * LOAD_ARG  copies globals → pending local frame BEFORE call_depth increases.
     * STORE_RET copies locals → globals AFTER RET has decremented call_depth.
     * -------------------------------------------------------------------- */
    OP_CALL         = 0x80,   /* [u16 abs_addr]               jump to function        */
    OP_RET          = 0x81,   /* (no operands)                return from function     */
    OP_LOAD_ARG     = 0x82,   /* [u8 local_id][u8 global_id]  stage arg before CALL   */
    OP_STORE_RET    = 0x83,   /* [u8 global_id][u8 local_id]  copy ret value to global*/
};