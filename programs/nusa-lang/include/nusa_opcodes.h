/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 danko1122q */

#pragma once
#include <stdint.h>

static const char    NUSA_MAGIC[6]    = {'\x7f', 'N', 'U', 'S', 'A', 0x01};
static const uint8_t NUSA_MAX_VARS    = 200;
static const size_t  NUSA_MAX_STR_LEN = 254;

enum NusaOpcode : uint8_t {
    OP_NOP         = 0x00,
    OP_PRINT_STR   = 0x01,
    OP_PRINT_VAR   = 0x02,
    OP_SET_STR     = 0x03,
    OP_SET_INT     = 0x04,
    OP_ADD_IMM     = 0x05,
    OP_SUB_IMM     = 0x06,
    OP_MUL_IMM     = 0x07,
    OP_DIV_IMM     = 0x08,
    OP_ADD_VAR     = 0x10,
    OP_SUB_VAR     = 0x11,
    OP_MUL_VAR     = 0x12,
    OP_DIV_VAR     = 0x13,
    OP_JMP_FWD     = 0x20,
    OP_JMP_IF_NZ   = 0x21,
    OP_JMP_IF_Z    = 0x22,
    OP_JMP_IF_NE   = 0x23,
    OP_JMP_IF_EQ   = 0x24,
    OP_JMP_BACK    = 0x30,
    OP_JMP_BACK_NZ = 0x31,
    OP_JMP_BACK_Z  = 0x32,
    OP_HALT        = 0xFF,
};