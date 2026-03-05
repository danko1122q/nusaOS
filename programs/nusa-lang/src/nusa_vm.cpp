/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 danko1122q */

#include "nusa_vm.h"
#include "nusa_opcodes.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <vector>

namespace NusaVM {

enum VarType : uint8_t { VAR_UNSET = 0, VAR_INT, VAR_STR };

struct VarSlot {
    VarType type;
    int32_t ival;
    char    sval[NUSA_MAX_STR_LEN + 1];
};

static bool read_u8(const std::vector<uint8_t>& bc, size_t& ip, uint8_t& out) {
    if (ip >= bc.size()) return false;
    out = bc[ip++];
    return true;
}

static bool read_u16(const std::vector<uint8_t>& bc, size_t& ip, uint16_t& out) {
    if (ip + 2 > bc.size()) return false;
    out = (uint16_t)(bc[ip] | ((uint16_t)bc[ip + 1] << 8));
    ip += 2;
    return true;
}

static bool read_i32(const std::vector<uint8_t>& bc, size_t& ip, int32_t& out) {
    if (ip + 4 > bc.size()) return false;
    out = (int32_t)((uint32_t)bc[ip]
                  | ((uint32_t)bc[ip+1] <<  8)
                  | ((uint32_t)bc[ip+2] << 16)
                  | ((uint32_t)bc[ip+3] << 24));
    ip += 4;
    return true;
}

#ifndef NUSA_NO_LOOP_LIMIT
static const uint32_t MAX_BACK_JUMPS = 1000000UL;
#endif

int run(const std::vector<uint8_t>& bc, int sym_count, const char* prog) {
    if (sym_count < 0 || sym_count > (int)NUSA_MAX_VARS) {
        fprintf(stderr, "%s: runtime error: invalid sym_count %d\n", prog, sym_count);
        return 1;
    }

    VarSlot vars[NUSA_MAX_VARS];
    for (int i = 0; i < sym_count; i++) {
        vars[i].type = VAR_UNSET;
        vars[i].ival = 0;
        memset(vars[i].sval, 0, sizeof(vars[i].sval));
    }

    size_t ip = 0;

#ifndef NUSA_NO_LOOP_LIMIT
    uint32_t back_jump_count = 0;
#endif

#define RT_ERR(msg) \
    do { fprintf(stderr, "%s: runtime error: %s\n", prog, msg); return 1; } while(0)

#define RT_ERR1(fmt, a) \
    do { fprintf(stderr, "%s: runtime error: " fmt "\n", prog, a); return 1; } while(0)

#define RT_ERR2(fmt, a, b) \
    do { fprintf(stderr, "%s: runtime error: " fmt "\n", prog, a, b); return 1; } while(0)

    while (ip < bc.size()) {
        uint8_t op = bc[ip++];

        switch ((NusaOpcode)op) {

        case OP_NOP:
            break;

        case OP_HALT:
            return 0;

        case OP_PRINT_STR: {
            uint8_t len;
            if (!read_u8(bc, ip, len))   RT_ERR("PRINT_STR: truncated length");
            if (ip + len > bc.size())    RT_ERR("PRINT_STR: truncated data");
            char buf[NUSA_MAX_STR_LEN + 1];
            memcpy(buf, &bc[ip], len);
            buf[len] = '\0';
            ip += len;
            printf("%s\n", buf);
            break;
        }

        case OP_PRINT_VAR: {
            uint8_t id;
            if (!read_u8(bc, ip, id))               RT_ERR("PRINT_VAR: truncated");
            if ((int)id >= sym_count)               RT_ERR1("PRINT_VAR: var id %u out of range", (unsigned)id);
            switch (vars[id].type) {
                case VAR_INT:  printf("%d\n",  vars[id].ival);  break;
                case VAR_STR:  printf("%s\n",  vars[id].sval);  break;
                default:       printf("(unset)\n");             break;
            }
            break;
        }

        case OP_SET_STR: {
            uint8_t id, len;
            if (!read_u8(bc, ip, id))               RT_ERR("SET_STR: truncated id");
            if ((int)id >= sym_count)               RT_ERR1("SET_STR: var id %u out of range", (unsigned)id);
            if (!read_u8(bc, ip, len))              RT_ERR("SET_STR: truncated length");
            if (len > NUSA_MAX_STR_LEN)             RT_ERR1("SET_STR: string too long (%u)", (unsigned)len);
            if (ip + len > bc.size())               RT_ERR("SET_STR: truncated data");
            vars[id].type = VAR_STR;
            memset(vars[id].sval, 0, sizeof(vars[id].sval));
            memcpy(vars[id].sval, &bc[ip], len);
            vars[id].sval[len] = '\0';
            ip += len;
            break;
        }

        case OP_SET_INT: {
            uint8_t id;
            int32_t val;
            if (!read_u8(bc, ip, id))               RT_ERR("SET_INT: truncated id");
            if ((int)id >= sym_count)               RT_ERR1("SET_INT: var id %u out of range", (unsigned)id);
            if (!read_i32(bc, ip, val))             RT_ERR("SET_INT: truncated value");
            vars[id].type = VAR_INT;
            vars[id].ival = val;
            break;
        }

        case OP_ADD_IMM:
        case OP_SUB_IMM:
        case OP_MUL_IMM:
        case OP_DIV_IMM: {
            uint8_t id;
            int32_t imm;
            if (!read_u8(bc, ip, id))               RT_ERR("ARITH_IMM: truncated id");
            if ((int)id >= sym_count)               RT_ERR1("ARITH_IMM: var id %u out of range", (unsigned)id);
            if (vars[id].type != VAR_INT)           RT_ERR1("ARITH_IMM: var %u is not integer", (unsigned)id);
            if (!read_i32(bc, ip, imm))             RT_ERR("ARITH_IMM: truncated immediate");
            if (op == OP_DIV_IMM && imm == 0)       RT_ERR("division by zero");
            switch ((NusaOpcode)op) {
                case OP_ADD_IMM: vars[id].ival += imm; break;
                case OP_SUB_IMM: vars[id].ival -= imm; break;
                case OP_MUL_IMM: vars[id].ival *= imm; break;
                case OP_DIV_IMM: vars[id].ival /= imm; break;
                default: break;
            }
            break;
        }

        case OP_ADD_VAR:
        case OP_SUB_VAR:
        case OP_MUL_VAR:
        case OP_DIV_VAR: {
            uint8_t dst, src;
            if (!read_u8(bc, ip, dst))              RT_ERR("ARITH_VAR: truncated dst");
            if ((int)dst >= sym_count)              RT_ERR1("ARITH_VAR: dst id %u out of range", (unsigned)dst);
            if (vars[dst].type != VAR_INT)          RT_ERR1("ARITH_VAR: dst %u is not integer", (unsigned)dst);
            if (!read_u8(bc, ip, src))              RT_ERR("ARITH_VAR: truncated src");
            if ((int)src >= sym_count)              RT_ERR1("ARITH_VAR: src id %u out of range", (unsigned)src);
            if (vars[src].type != VAR_INT)          RT_ERR1("ARITH_VAR: src %u is not integer", (unsigned)src);
            if (op == OP_DIV_VAR && vars[src].ival == 0) RT_ERR("division by zero");
            switch ((NusaOpcode)op) {
                case OP_ADD_VAR: vars[dst].ival += vars[src].ival; break;
                case OP_SUB_VAR: vars[dst].ival -= vars[src].ival; break;
                case OP_MUL_VAR: vars[dst].ival *= vars[src].ival; break;
                case OP_DIV_VAR: vars[dst].ival /= vars[src].ival; break;
                default: break;
            }
            break;
        }

        case OP_JMP_FWD: {
            uint16_t offset;
            if (!read_u16(bc, ip, offset))          RT_ERR("JMP_FWD: truncated offset");
            if (ip + offset > bc.size())            RT_ERR("JMP_FWD: jumps past end");
            ip += offset;
            break;
        }

        case OP_JMP_IF_NZ:
        case OP_JMP_IF_Z: {
            uint8_t  id;
            uint16_t offset;
            if (!read_u8(bc, ip, id))               RT_ERR("JMP_IF: truncated id");
            if ((int)id >= sym_count)               RT_ERR1("JMP_IF: var id %u out of range", (unsigned)id);
            if (!read_u16(bc, ip, offset))          RT_ERR("JMP_IF: truncated offset");
            int32_t val = (vars[id].type == VAR_INT) ? vars[id].ival
                        : (vars[id].type == VAR_STR && vars[id].sval[0]) ? 1 : 0;
            bool take = (op == OP_JMP_IF_NZ) ? (val != 0) : (val == 0);
            if (take) {
                if (ip + offset > bc.size())        RT_ERR("JMP_IF: jumps past end");
                ip += offset;
            }
            break;
        }

        case OP_JMP_IF_NE:
        case OP_JMP_IF_EQ: {
            uint8_t  id;
            int32_t  cmp_val;
            uint16_t offset;
            if (!read_u8(bc, ip, id))               RT_ERR("JMP_CMP: truncated id");
            if ((int)id >= sym_count)               RT_ERR1("JMP_CMP: var id %u out of range", (unsigned)id);
            if (!read_i32(bc, ip, cmp_val))         RT_ERR("JMP_CMP: truncated cmp value");
            if (!read_u16(bc, ip, offset))          RT_ERR("JMP_CMP: truncated offset");
            int32_t val = (vars[id].type == VAR_INT) ? vars[id].ival : 0;
            bool take = (op == OP_JMP_IF_NE) ? (val != cmp_val) : (val == cmp_val);
            if (take) {
                if (ip + offset > bc.size())        RT_ERR("JMP_CMP: jumps past end");
                ip += offset;
            }
            break;
        }

        case OP_JMP_BACK: {
            uint16_t offset;
            if (!read_u16(bc, ip, offset))          RT_ERR("JMP_BACK: truncated offset");
#ifndef NUSA_NO_LOOP_LIMIT
            if (++back_jump_count > MAX_BACK_JUMPS) RT_ERR("infinite loop detected");
#endif
            if (offset > ip)                        RT_ERR("JMP_BACK: underflows bytecode");
            ip -= offset;
            break;
        }

        case OP_JMP_BACK_NZ:
        case OP_JMP_BACK_Z: {
            uint8_t  id;
            uint16_t offset;
            if (!read_u8(bc, ip, id))               RT_ERR("JMP_BACK_NZ/Z: truncated id");
            if ((int)id >= sym_count)               RT_ERR1("JMP_BACK_NZ/Z: var id %u out of range", (unsigned)id);
            if (!read_u16(bc, ip, offset))          RT_ERR("JMP_BACK_NZ/Z: truncated offset");
            int32_t val = (vars[id].type == VAR_INT) ? vars[id].ival : 0;
            bool take = (op == OP_JMP_BACK_NZ) ? (val != 0) : (val == 0);
            if (take) {
#ifndef NUSA_NO_LOOP_LIMIT
                if (++back_jump_count > MAX_BACK_JUMPS) RT_ERR("infinite loop detected");
#endif
                if (offset > ip)                    RT_ERR("JMP_BACK_NZ/Z: underflows bytecode");
                ip -= offset;
            }
            break;
        }

        default:
            RT_ERR2("unknown opcode 0x%02X at offset %zu", (unsigned)op, ip - 1);
        }
    }

#undef RT_ERR
#undef RT_ERR1
#undef RT_ERR2

    return 0;
}

} // namespace NusaVM