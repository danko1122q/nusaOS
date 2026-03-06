/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 danko1122q / nusaOS project */

#pragma once
#include <stdint.h>
#include <stddef.h>

/* ── Magic & limits ───────────────────────────────────────────────────── */
static const char    NSA_MAGIC[6]     = {'\x7f', 'N', 'S', 'A', 0x02, 0x00};
static const uint8_t NSA_MAX_VARS     = 200;
static const size_t  NSA_MAX_STR_LEN  = 254;
static const char    NSA_EXT[]        = ".nsa";
static const char    NSA_BIN_EXT[]    = ".nbin";

/* ── Opcode table ─────────────────────────────────────────────────────── */
enum NsaOpcode : uint8_t {
    /* Control */
    OP_NOP          = 0x00,
    OP_HALT         = 0xFF,

    /* I/O */
    OP_PRINT_STR    = 0x01,   /* [u8 len][bytes...]          print literal string + newline  */
    OP_PRINT_VAR    = 0x02,   /* [u8 id]                     print variable + newline        */
    OP_PRINT_STR_NL = 0x03,   /* [u8 len][bytes...]          print literal, NO newline       */
    OP_PRINT_VAR_NL = 0x04,   /* [u8 id]                     print variable, NO newline      */
    OP_INPUT_INT    = 0x05,   /* [u8 id]                     read int32 from stdin           */
    OP_INPUT_STR    = 0x06,   /* [u8 id]                     read line from stdin            */

    /* Variables */
    OP_SET_STR      = 0x10,   /* [u8 id][u8 len][bytes...]   var = literal string            */
    OP_SET_INT      = 0x11,   /* [u8 id][i32 val]            var = literal int               */
    OP_SET_BOOL     = 0x12,   /* [u8 id][u8 0|1]             var = true/false                */
    OP_COPY         = 0x13,   /* [u8 dst][u8 src]            dst = src (any type)            */

    /* Integer arithmetic (immediate) */
    OP_ADD_IMM      = 0x20,   /* [u8 id][i32]  */
    OP_SUB_IMM      = 0x21,
    OP_MUL_IMM      = 0x22,
    OP_DIV_IMM      = 0x23,
    OP_MOD_IMM      = 0x24,   /* modulo — new  */

    /* Integer arithmetic (var) */
    OP_ADD_VAR      = 0x28,   /* [u8 dst][u8 src] */
    OP_SUB_VAR      = 0x29,
    OP_MUL_VAR      = 0x2A,
    OP_DIV_VAR      = 0x2B,
    OP_MOD_VAR      = 0x2C,   /* modulo — new  */

    /* Unary */
    OP_INC          = 0x30,   /* [u8 id]  id++  */
    OP_DEC          = 0x31,   /* [u8 id]  id--  */
    OP_NOT          = 0x32,   /* [u8 id]  id = !id (int: 0↔1, bool: flip) */
    OP_NEG          = 0x33,   /* [u8 id]  id = -id  */

    /* String ops */
    OP_CONCAT       = 0x40,   /* [u8 dst][u8 src]  dst = dst + src (string concat) */
    OP_CONCAT_LIT   = 0x41,   /* [u8 dst][u8 len][bytes...]  dst += literal         */
    OP_LEN          = 0x42,   /* [u8 dst_int][u8 src_str]   dst = len(src)          */
    OP_STR_TO_INT   = 0x43,   /* [u8 dst_int][u8 src_str]   dst = int(src)          */
    OP_INT_TO_STR   = 0x44,   /* [u8 dst_str][u8 src_int]   dst = str(src)          */

    /* Comparison → bool result */
    OP_CMP_EQ       = 0x50,   /* [u8 dst_bool][u8 a][u8 b]  dst = (a == b)  */
    OP_CMP_NE       = 0x51,
    OP_CMP_LT       = 0x52,
    OP_CMP_GT       = 0x53,
    OP_CMP_LE       = 0x54,
    OP_CMP_GE       = 0x55,

    /* Logical */
    OP_AND          = 0x58,   /* [u8 dst][u8 a][u8 b] */
    OP_OR           = 0x59,

    /* Forward jumps (offset = u16, bytes to skip AFTER the jump instruction) */
    OP_JMP_FWD      = 0x60,   /* [u16 offset]                     unconditional    */
    OP_JMP_IF_TRUE  = 0x61,   /* [u8 id][u16 offset]  jump if var is truthy        */
    OP_JMP_IF_FALSE = 0x62,   /* [u8 id][u16 offset]  jump if var is falsy         */
    OP_JMP_IF_EQ    = 0x63,   /* [u8 id][i32 cmp][u16 offset]                      */
    OP_JMP_IF_NE    = 0x64,
    OP_JMP_IF_LT    = 0x65,
    OP_JMP_IF_GT    = 0x66,
    OP_JMP_IF_LE    = 0x67,
    OP_JMP_IF_GE    = 0x68,

    /* Backward jumps (offset = u16, bytes to rewind INCLUDING the jump instruction) */
    OP_JMP_BACK         = 0x70,   /* [u16 offset]             unconditional         */
    OP_JMP_BACK_TRUE    = 0x71,   /* [u8 id][u16 offset]                            */
    OP_JMP_BACK_FALSE   = 0x72,
    OP_JMP_BACK_NZ      = 0x73,   /* kept for loop-counter compatibility            */
    OP_JMP_BACK_Z       = 0x74,
};