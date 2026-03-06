/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 danko1122q / nusaOS project */

#include "nsa_compiler.h"
#include "nsa_lexer.h"
#include "nsa_opcodes.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include <sstream>

namespace NsaCompiler {

/* ── Symbol table ──────────────────────────────────────────────────────── */
enum SymType { SYM_INT, SYM_STR, SYM_BOOL };

struct Symbol {
    uint8_t id;
    SymType type;
    bool    is_internal; /* compiler-generated temp */
};

/* ── Block / control-flow stack ────────────────────────────────────────── */
enum BlockKind {
    BLK_IF,
    BLK_ELSE,
    BLK_LOOP_TIMES,
    BLK_LOOP_WHILE
};

struct Block {
    BlockKind kind;
    size_t    fwd_patch_pos;   /* position of u16 to patch for forward jump */
    size_t    loop_start;      /* bytecode offset of top-of-loop           */
    uint8_t   counter_id;      /* loop counter variable id                  */
    bool      owns_counter;    /* true  → compiler-allocated counter        */
    /* For JMP_IF_EQ/NE/LT etc. the opcode already has i32 embedded,
       so fwd_patch_pos points to the u16 offset field directly.           */
};

/* ── Compiler state ─────────────────────────────────────────────────────── */
struct CS {
    std::map<std::string, Symbol> symbols;
    uint8_t                       next_id   = 0;
    std::vector<uint8_t>          bytecode;
    std::vector<Block>            blk_stack;
    int                           line      = 0;
    int                           errors    = 0;
    int                           warnings  = 0;
    const char*                   filename  = "<input>";

    void emit(uint8_t b)       { bytecode.push_back(b); }
    void emit_u8(uint8_t b)    { bytecode.push_back(b); }

    void emit_u16(uint16_t v) {
        bytecode.push_back((uint8_t)( v       & 0xFF));
        bytecode.push_back((uint8_t)((v >> 8) & 0xFF));
    }

    void emit_i32(int32_t v) {
        bytecode.push_back((uint8_t)( v        & 0xFF));
        bytecode.push_back((uint8_t)((v >>  8) & 0xFF));
        bytecode.push_back((uint8_t)((v >> 16) & 0xFF));
        bytecode.push_back((uint8_t)((v >> 24) & 0xFF));
    }

    void patch_u16(size_t pos, uint16_t v) {
        bytecode[pos]     = (uint8_t)( v       & 0xFF);
        bytecode[pos + 1] = (uint8_t)((v >> 8) & 0xFF);
    }

    size_t here() const { return bytecode.size(); }

    void error(const std::string& msg) {
        fprintf(stderr, "%s:%d: error: %s\n", filename, line, msg.c_str());
        errors++;
    }

    void warn(const std::string& msg) {
        fprintf(stderr, "%s:%d: warning: %s\n", filename, line, msg.c_str());
        warnings++;
    }

    /* Lookup existing symbol */
    bool lookup(const std::string& name, uint8_t& id_out, SymType* type_out = nullptr) {
        auto it = symbols.find(name);
        if (it == symbols.end()) {
            error("undeclared variable '" + name + "'");
            return false;
        }
        id_out = it->second.id;
        if (type_out) *type_out = it->second.type;
        return true;
    }

    /* Intern a user variable (create or retrieve, type-checked) */
    bool intern(const std::string& name, SymType type, uint8_t& id_out) {
        auto it = symbols.find(name);
        if (it != symbols.end()) {
            if (it->second.type != type) {
                error("'" + name + "' was previously declared as "
                      + sym_type_name(it->second.type)
                      + ", cannot redeclare as " + sym_type_name(type));
                return false;
            }
            id_out = it->second.id;
            return true;
        }
        if (next_id >= NSA_MAX_VARS) { error("too many variables (max 200)"); return false; }
        id_out = next_id++;
        Symbol s; s.id = id_out; s.type = type; s.is_internal = false;
        symbols[name] = s;
        return true;
    }

    /* Allocate a compiler-internal temporary */
    bool alloc_temp(uint8_t& id_out) {
        if (next_id >= NSA_MAX_VARS) { error("too many variables"); return false; }
        id_out = next_id++;
        return true;
    }

    static const char* sym_type_name(SymType t) {
        switch (t) {
            case SYM_INT:  return "int";
            case SYM_STR:  return "string";
            case SYM_BOOL: return "bool";
        }
        return "unknown";
    }
};

/* ── Convenience aliases ─────────────────────────────────────────────── */
using Tok  = NsaLexer::Token;
using Toks = std::vector<Tok>;
using TK   = NsaLexer::TokenKind;

static bool is_ident(const Tok& t, const char* s) {
    return t.kind == TK::TK_IDENT && t.text == s;
}

/* Emit a literal string body (len byte + bytes) */
static void emit_str_literal(CS& cs, const std::string& s) {
    cs.emit((uint8_t)s.size());
    for (size_t i = 0; i < s.size(); i++) cs.emit((uint8_t)s[i]);
}

/* ─────────────────────── Statement parsers ──────────────────────────── */

/* let <name> = <value>
   let <name> = true|false   */
static void parse_let(CS& cs, const Toks& t) {
    if (t.size() < 4) { cs.error("expected: let <name> = <value>"); return; }
    if (t[1].kind != TK::TK_IDENT) { cs.error("expected variable name after 'let'"); return; }
    const std::string& name = t[1].text;
    if (NsaLexer::is_keyword(name)) { cs.error("'" + name + "' is a reserved keyword"); return; }
    if (t[2].kind != TK::TK_OP || t[2].text != "=") { cs.error("expected '=' after variable name"); return; }

    const Tok& val = t[3];

    if (val.kind == TK::TK_BOOL) {
        uint8_t id;
        if (!cs.intern(name, SYM_BOOL, id)) return;
        cs.emit(OP_SET_BOOL); cs.emit(id); cs.emit((uint8_t)val.ival);
        return;
    }

    if (val.kind == TK::TK_STRING) {
        if (val.text.size() > NSA_MAX_STR_LEN) { cs.error("string literal too long (max 254 chars)"); return; }
        uint8_t id;
        if (!cs.intern(name, SYM_STR, id)) return;
        cs.emit(OP_SET_STR); cs.emit(id);
        emit_str_literal(cs, val.text);
        return;
    }

    if (val.kind == TK::TK_INT) {
        uint8_t id;
        if (!cs.intern(name, SYM_INT, id)) return;
        cs.emit(OP_SET_INT); cs.emit(id); cs.emit_i32(val.ival);
        return;
    }

    /* let x = y  (copy from variable) */
    if (val.kind == TK::TK_IDENT && !NsaLexer::is_keyword(val.text)) {
        uint8_t src; SymType src_type;
        if (!cs.lookup(val.text, src, &src_type)) return;
        uint8_t dst;
        if (!cs.intern(name, src_type, dst)) return;
        cs.emit(OP_COPY); cs.emit(dst); cs.emit(src);
        return;
    }

    cs.error("expected integer, string, bool, or variable after '='");
}

/* copy <dst> <src> */
static void parse_copy(CS& cs, const Toks& t) {
    if (t.size() < 3) { cs.error("expected: copy <dst> <src>"); return; }
    if (t[1].kind != TK::TK_IDENT || NsaLexer::is_keyword(t[1].text)) {
        cs.error("expected destination variable"); return;
    }
    if (t[2].kind != TK::TK_IDENT || NsaLexer::is_keyword(t[2].text)) {
        cs.error("expected source variable"); return;
    }
    uint8_t src; SymType src_type;
    if (!cs.lookup(t[2].text, src, &src_type)) return;
    uint8_t dst;
    if (!cs.intern(t[1].text, src_type, dst)) return;
    cs.emit(OP_COPY); cs.emit(dst); cs.emit(src);
}

/* print <expr>      (with newline)
   println <expr>    (without newline)  */
static void parse_print(CS& cs, const Toks& t, bool newline) {
    if (t.size() < 2) { cs.error("expected argument after 'print'"); return; }
    const Tok& arg = t[1];

    NsaOpcode str_op = newline ? OP_PRINT_STR    : OP_PRINT_STR_NL;
    NsaOpcode var_op = newline ? OP_PRINT_VAR    : OP_PRINT_VAR_NL;

    if (arg.kind == TK::TK_STRING) {
        if (arg.text.size() > NSA_MAX_STR_LEN) { cs.error("string literal too long"); return; }
        cs.emit(str_op);
        emit_str_literal(cs, arg.text);
        return;
    }

    if (arg.kind == TK::TK_IDENT && !NsaLexer::is_keyword(arg.text)) {
        uint8_t id;
        if (!cs.lookup(arg.text, id)) return;
        cs.emit(var_op); cs.emit(id);
        return;
    }

    cs.error("expected string literal or variable after 'print'");
}

/* input <var>   — reads from stdin.
   If var is int, parse integer. If str, read line.
   If var not yet declared, defaults to string. */
static void parse_input(CS& cs, const Toks& t) {
    if (t.size() < 2 || t[1].kind != TK::TK_IDENT || NsaLexer::is_keyword(t[1].text)) {
        cs.error("expected: input <variable>"); return;
    }
    const std::string& name = t[1].text;

    auto it = cs.symbols.find(name);
    if (it == cs.symbols.end()) {
        /* Auto-declare as string */
        uint8_t id;
        if (!cs.intern(name, SYM_STR, id)) return;
        cs.emit(OP_INPUT_STR); cs.emit(id);
        return;
    }

    switch (it->second.type) {
        case SYM_INT:
            cs.emit(OP_INPUT_INT); cs.emit(it->second.id); break;
        case SYM_STR:
            cs.emit(OP_INPUT_STR); cs.emit(it->second.id); break;
        case SYM_BOOL:
            cs.error("'input' into a bool variable is not supported (use int/str)"); break;
    }
}

/* inc <var>  /  dec <var> */
static void parse_inc_dec(CS& cs, const Toks& t, NsaOpcode op) {
    if (t.size() < 2 || t[1].kind != TK::TK_IDENT || NsaLexer::is_keyword(t[1].text)) {
        cs.error("expected: inc/dec <variable>"); return;
    }
    uint8_t id; SymType type;
    if (!cs.lookup(t[1].text, id, &type)) return;
    if (type != SYM_INT) { cs.error("'" + t[1].text + "' is not an integer"); return; }
    cs.emit(op); cs.emit(id);
}

/* not <var>  /  neg <var> */
static void parse_unary(CS& cs, const Toks& t, NsaOpcode op) {
    if (t.size() < 2 || t[1].kind != TK::TK_IDENT || NsaLexer::is_keyword(t[1].text)) {
        cs.error("expected variable after unary op"); return;
    }
    uint8_t id; SymType type;
    if (!cs.lookup(t[1].text, id, &type)) return;
    if (op == OP_NEG && type != SYM_INT) { cs.error("'neg' requires integer"); return; }
    if (op == OP_NOT && type != SYM_INT && type != SYM_BOOL) {
        cs.error("'not' requires integer or bool"); return;
    }
    cs.emit(op); cs.emit(id);
}

/* Arithmetic: add/sub/mul/div/mod <dst> <int|var> */
struct ArithDef { const char* kw; NsaOpcode op_imm; NsaOpcode op_var; };
static const ArithDef ARITH_TABLE[] = {
    {"add", OP_ADD_IMM, OP_ADD_VAR},
    {"sub", OP_SUB_IMM, OP_SUB_VAR},
    {"mul", OP_MUL_IMM, OP_MUL_VAR},
    {"div", OP_DIV_IMM, OP_DIV_VAR},
    {"mod", OP_MOD_IMM, OP_MOD_VAR},
    {nullptr, OP_NOP, OP_NOP}
};

static void parse_arith(CS& cs, const Toks& t, const ArithDef& def) {
    if (t.size() < 3) { cs.error(std::string(def.kw) + ": expected <var> <int|var>"); return; }
    if (t[1].kind != TK::TK_IDENT || NsaLexer::is_keyword(t[1].text)) {
        cs.error("expected destination variable"); return;
    }
    uint8_t dst_id; SymType dst_type;
    if (!cs.lookup(t[1].text, dst_id, &dst_type)) return;
    if (dst_type != SYM_INT) { cs.error("'" + t[1].text + "' is not an integer"); return; }

    const Tok& operand = t[2];
    if (operand.kind == TK::TK_INT) {
        if ((def.op_imm == OP_DIV_IMM || def.op_imm == OP_MOD_IMM) && operand.ival == 0) {
            cs.error("division/modulo by zero"); return;
        }
        cs.emit(def.op_imm); cs.emit(dst_id); cs.emit_i32(operand.ival);
    } else if (operand.kind == TK::TK_IDENT && !NsaLexer::is_keyword(operand.text)) {
        uint8_t src_id; SymType src_type;
        if (!cs.lookup(operand.text, src_id, &src_type)) return;
        if (src_type != SYM_INT) { cs.error("'" + operand.text + "' is not an integer"); return; }
        cs.emit(def.op_var); cs.emit(dst_id); cs.emit(src_id);
    } else {
        cs.error("expected integer literal or variable as operand");
    }
}

/* concat <dst_str> <src_str|"literal"> */
static void parse_concat(CS& cs, const Toks& t) {
    if (t.size() < 3) { cs.error("expected: concat <dst> <src|\"literal\">"); return; }
    if (t[1].kind != TK::TK_IDENT || NsaLexer::is_keyword(t[1].text)) {
        cs.error("expected destination string variable"); return;
    }
    uint8_t dst; SymType dtype;
    if (!cs.lookup(t[1].text, dst, &dtype)) return;
    if (dtype != SYM_STR) { cs.error("'" + t[1].text + "' is not a string"); return; }

    if (t[2].kind == TK::TK_STRING) {
        if (t[2].text.size() > NSA_MAX_STR_LEN) { cs.error("string literal too long"); return; }
        cs.emit(OP_CONCAT_LIT); cs.emit(dst);
        emit_str_literal(cs, t[2].text);
    } else if (t[2].kind == TK::TK_IDENT && !NsaLexer::is_keyword(t[2].text)) {
        uint8_t src; SymType stype;
        if (!cs.lookup(t[2].text, src, &stype)) return;
        if (stype != SYM_STR) { cs.error("'" + t[2].text + "' is not a string"); return; }
        cs.emit(OP_CONCAT); cs.emit(dst); cs.emit(src);
    } else {
        cs.error("expected string variable or literal as second argument to 'concat'");
    }
}

/* len <dst_int> <src_str> */
static void parse_len(CS& cs, const Toks& t) {
    if (t.size() < 3) { cs.error("expected: len <int_var> <str_var>"); return; }
    uint8_t dst; SymType dtype;
    if (t[1].kind != TK::TK_IDENT || NsaLexer::is_keyword(t[1].text)) {
        cs.error("expected integer destination variable"); return;
    }
    /* dst is allowed to be undeclared — auto-declare as int */
    auto it = cs.symbols.find(t[1].text);
    if (it == cs.symbols.end()) {
        if (!cs.intern(t[1].text, SYM_INT, dst)) return;
    } else {
        dst   = it->second.id;
        dtype = it->second.type;
        if (dtype != SYM_INT) { cs.error("'" + t[1].text + "' is not an integer"); return; }
    }

    if (t[2].kind != TK::TK_IDENT || NsaLexer::is_keyword(t[2].text)) {
        cs.error("expected string source variable"); return;
    }
    uint8_t src; SymType stype;
    if (!cs.lookup(t[2].text, src, &stype)) return;
    if (stype != SYM_STR) { cs.error("'" + t[2].text + "' is not a string"); return; }
    cs.emit(OP_LEN); cs.emit(dst); cs.emit(src);
}

/* to_int <dst_int> <src_str>  — parse string as integer */
static void parse_to_int(CS& cs, const Toks& t) {
    if (t.size() < 3) { cs.error("expected: to_int <int_var> <str_var>"); return; }
    uint8_t dst;
    if (t[1].kind != TK::TK_IDENT || NsaLexer::is_keyword(t[1].text)) {
        cs.error("expected integer destination variable"); return;
    }
    if (!cs.intern(t[1].text, SYM_INT, dst)) return;
    uint8_t src; SymType stype;
    if (!cs.lookup(t[2].text, src, &stype)) return;
    if (stype != SYM_STR) { cs.error("'" + t[2].text + "' is not a string"); return; }
    cs.emit(OP_STR_TO_INT); cs.emit(dst); cs.emit(src);
}

/* to_str <dst_str> <src_int>  — convert integer to string */
static void parse_to_str(CS& cs, const Toks& t) {
    if (t.size() < 3) { cs.error("expected: to_str <str_var> <int_var>"); return; }
    uint8_t dst;
    if (t[1].kind != TK::TK_IDENT || NsaLexer::is_keyword(t[1].text)) {
        cs.error("expected string destination variable"); return;
    }
    if (!cs.intern(t[1].text, SYM_STR, dst)) return;
    uint8_t src; SymType stype;
    if (!cs.lookup(t[2].text, src, &stype)) return;
    if (stype != SYM_INT) { cs.error("'" + t[2].text + "' is not an integer"); return; }
    cs.emit(OP_INT_TO_STR); cs.emit(dst); cs.emit(src);
}

/* cmp <dst_bool> <a> <op> <b>
   Emits OP_CMP_EQ/NE/LT/GT/LE/GE dst a b  */
static void parse_cmp(CS& cs, const Toks& t) {
    if (t.size() < 5) { cs.error("expected: cmp <bool_var> <a> <op> <b>"); return; }
    if (t[1].kind != TK::TK_IDENT || NsaLexer::is_keyword(t[1].text)) {
        cs.error("expected bool destination variable"); return;
    }
    uint8_t dst;
    if (!cs.intern(t[1].text, SYM_BOOL, dst)) return;

    uint8_t va; SymType vta;
    if (!cs.lookup(t[2].text, va, &vta)) return;
    if (vta != SYM_INT) { cs.error("comparison operands must be integer"); return; }

    if (t[3].kind != TK::TK_OP) { cs.error("expected comparison operator"); return; }
    const std::string& op = t[3].text;

    uint8_t vb; SymType vtb;
    if (!cs.lookup(t[4].text, vb, &vtb)) return;
    if (vtb != SYM_INT) { cs.error("comparison operands must be integer"); return; }

    NsaOpcode cmp_op;
    if      (op == "==") cmp_op = OP_CMP_EQ;
    else if (op == "!=") cmp_op = OP_CMP_NE;
    else if (op == "<")  cmp_op = OP_CMP_LT;
    else if (op == ">")  cmp_op = OP_CMP_GT;
    else if (op == "<=") cmp_op = OP_CMP_LE;
    else if (op == ">=") cmp_op = OP_CMP_GE;
    else { cs.error("unknown comparison operator '" + op + "'"); return; }

    cs.emit(cmp_op); cs.emit(dst); cs.emit(va); cs.emit(vb);
}

/* and <dst> <a> <b>  /  or <dst> <a> <b> */
static void parse_logical(CS& cs, const Toks& t, NsaOpcode op) {
    if (t.size() < 4) { cs.error("expected: and/or <dst> <a> <b>"); return; }
    uint8_t dst;
    if (!cs.intern(t[1].text, SYM_BOOL, dst)) return;
    uint8_t va; if (!cs.lookup(t[2].text, va)) return;
    uint8_t vb; if (!cs.lookup(t[3].text, vb)) return;
    cs.emit(op); cs.emit(dst); cs.emit(va); cs.emit(vb);
}

/* ─────────────────────── Control flow ─────────────────────────────── */

/* Helper: parse a condition and emit the appropriate conditional jump.
   Returns false on parse error.
   Condition forms supported:
     <var>              (truthy test)
     <var> <op> <int>   (compare var to literal)
   On success, patches fwd_patch_pos and sets block accordingly.        */
static bool emit_condition_jump(CS& cs, const Toks& t, size_t cond_start,
                                 size_t cond_end, /* exclusive */
                                 bool jump_if_true,  /* true → skip body when TRUE (loop exit)
                                                        false→ skip body when FALSE (if-entry skip) */
                                 Block& blk_out)
{
    /* cond_end == cond_start+1 → single variable (truthy) */
    if (cond_end - cond_start == 1) {
        if (t[cond_start].kind != TK::TK_IDENT || NsaLexer::is_keyword(t[cond_start].text)) {
            cs.error("expected variable in condition"); return false;
        }
        uint8_t id;
        if (!cs.lookup(t[cond_start].text, id)) return false;
        cs.emit(jump_if_true ? OP_JMP_IF_TRUE : OP_JMP_IF_FALSE);
        cs.emit(id);
        blk_out.fwd_patch_pos = cs.here();
        cs.emit_u16(0);
        return true;
    }

    /* <var> <op> <int> */
    if (cond_end - cond_start == 3) {
        if (t[cond_start].kind != TK::TK_IDENT || NsaLexer::is_keyword(t[cond_start].text)) {
            cs.error("expected variable in condition"); return false;
        }
        uint8_t id; SymType type;
        if (!cs.lookup(t[cond_start].text, id, &type)) return false;
        if (type != SYM_INT) { cs.error("comparison condition requires integer variable"); return false; }

        if (t[cond_start+1].kind != TK::TK_OP) { cs.error("expected comparison operator"); return false; }
        const std::string& op = t[cond_start+1].text;

        if (t[cond_start+2].kind != TK::TK_INT) { cs.error("expected integer literal on right side of condition"); return false; }
        int32_t cmp_val = t[cond_start+2].ival;

        /* Select the jump opcode:
           jump_if_true=false means "jump over body when condition is FALSE"
           so we need the NEGATED condition jump.                          */
        NsaOpcode jmp_op;
        if (!jump_if_true) {
            /* Skip body when condition FALSE → jump when NOT (op) */
            if      (op == "==") jmp_op = OP_JMP_IF_NE;
            else if (op == "!=") jmp_op = OP_JMP_IF_EQ;
            else if (op == "<")  jmp_op = OP_JMP_IF_GE;
            else if (op == ">")  jmp_op = OP_JMP_IF_LE;
            else if (op == "<=") jmp_op = OP_JMP_IF_GT;
            else if (op == ">=") jmp_op = OP_JMP_IF_LT;
            else { cs.error("unknown operator '" + op + "'"); return false; }
        } else {
            /* loop while: jump out when condition FALSE (condition means "keep looping") */
            if      (op == "==") jmp_op = OP_JMP_IF_NE;
            else if (op == "!=") jmp_op = OP_JMP_IF_EQ;
            else if (op == "<")  jmp_op = OP_JMP_IF_GE;
            else if (op == ">")  jmp_op = OP_JMP_IF_LE;
            else if (op == "<=") jmp_op = OP_JMP_IF_GT;
            else if (op == ">=") jmp_op = OP_JMP_IF_LT;
            else { cs.error("unknown operator '" + op + "'"); return false; }
        }

        cs.emit(jmp_op); cs.emit(id); cs.emit_i32(cmp_val);
        blk_out.fwd_patch_pos = cs.here();
        cs.emit_u16(0);
        return true;
    }

    cs.error("malformed condition — expected '<var>', or '<var> <op> <int>'");
    return false;
}

/* if <cond> then ... [else ...] end */
static void parse_if(CS& cs, const Toks& t) {
    /* Find 'then' */
    size_t then_pos = 0;
    for (size_t i = 1; i < t.size(); i++) {
        if (is_ident(t[i], "then")) { then_pos = i; break; }
    }
    if (then_pos == 0) { cs.error("expected 'then' in 'if' statement"); return; }

    Block blk;
    blk.kind = BLK_IF;
    blk.loop_start = 0; blk.counter_id = 0; blk.owns_counter = false;

    /* Emit jump-if-false (skip body when condition is false) */
    if (!emit_condition_jump(cs, t, 1, then_pos, false, blk)) return;
    cs.blk_stack.push_back(blk);
}

/* else */
static void parse_else(CS& cs, const Toks&) {
    if (cs.blk_stack.empty() || cs.blk_stack.back().kind != BLK_IF) {
        cs.error("'else' without matching 'if'"); return;
    }
    Block& blk = cs.blk_stack.back();

    /* Emit unconditional jump to skip else body */
    cs.emit(OP_JMP_FWD);
    size_t else_patch = cs.here();
    cs.emit_u16(0);

    /* Patch the if-jump to land here (start of else body) */
    size_t span = cs.here() - blk.fwd_patch_pos - 2;
    if (span > 0xFFFF) { cs.error("if-block too large"); return; }
    cs.patch_u16(blk.fwd_patch_pos, (uint16_t)span);

    blk.kind          = BLK_ELSE;
    blk.fwd_patch_pos = else_patch;
}

/* loop <N> times ... end
   loop while <cond> ... end  */
static void parse_loop(CS& cs, const Toks& t) {
    if (t.size() < 2) {
        cs.error("expected: 'loop <N> times' or 'loop while <cond>'"); return;
    }

    Block blk;
    blk.fwd_patch_pos = 0; blk.loop_start = 0;
    blk.counter_id = 0; blk.owns_counter = false;

    /* loop while <cond> */
    if (is_ident(t[1], "while")) {
        if (t.size() < 3) { cs.error("expected condition after 'loop while'"); return; }

        blk.kind = BLK_LOOP_WHILE;

        /* Record top-of-loop BEFORE emitting condition check */
        blk.loop_start = cs.here();

        /* Find end of condition (no 'then' keyword needed for loop while) */
        /* Condition is everything from t[2] to end of tokens */
        size_t cond_end = t.size();

        if (!emit_condition_jump(cs, t, 2, cond_end, true, blk)) return;
        cs.blk_stack.push_back(blk);
        return;
    }

    /* loop <N> times */
    if (t[1].kind == TK::TK_INT) {
        if (t.size() < 3 || !is_ident(t[2], "times")) {
            cs.error("expected 'times' after loop count"); return;
        }
        int32_t count = t[1].ival;
        if (count <= 0) { cs.error("loop count must be a positive integer"); return; }

        uint8_t ctr;
        if (!cs.alloc_temp(ctr)) return;

        cs.emit(OP_SET_INT); cs.emit(ctr); cs.emit_i32(count);

        blk.kind         = BLK_LOOP_TIMES;
        blk.loop_start   = cs.here();
        blk.counter_id   = ctr;
        blk.owns_counter = true;
        cs.blk_stack.push_back(blk);
        return;
    }

    cs.error("expected: 'loop <N> times' or 'loop while <cond>'");
}

/* end — closes nearest open block */
static void parse_end(CS& cs, const Toks&) {
    if (cs.blk_stack.empty()) { cs.error("'end' without matching 'if' or 'loop'"); return; }
    Block blk = cs.blk_stack.back();
    cs.blk_stack.pop_back();

    switch (blk.kind) {

    case BLK_IF:
    case BLK_ELSE: {
        size_t span = cs.here() - blk.fwd_patch_pos - 2;
        if (span > 0xFFFF) { cs.error("block body too large (>64KB)"); return; }
        cs.patch_u16(blk.fwd_patch_pos, (uint16_t)span);
        break;
    }

    case BLK_LOOP_TIMES: {
        /* Decrement counter, then jump back if still non-zero */
        cs.emit(OP_DEC); cs.emit(blk.counter_id);
        size_t after_instr = cs.here() + 1 /* OP */ + 1 /* id */ + 2 /* u16 */;
        if (after_instr < blk.loop_start) { cs.error("internal: bad loop_start"); return; }
        size_t dist = after_instr - blk.loop_start;
        if (dist > 0xFFFF) { cs.error("loop body too large (>64KB)"); return; }
        cs.emit(OP_JMP_BACK_NZ); cs.emit(blk.counter_id); cs.emit_u16((uint16_t)dist);
        break;
    }

    case BLK_LOOP_WHILE: {
        /* Unconditional jump back to top of loop (where condition is re-checked) */
        size_t after_instr = cs.here() + 1 /* OP */ + 2 /* u16 */;
        if (after_instr < blk.loop_start) { cs.error("internal: bad loop_start"); return; }
        size_t dist = after_instr - blk.loop_start;
        if (dist > 0xFFFF) { cs.error("loop body too large (>64KB)"); return; }
        cs.emit(OP_JMP_BACK); cs.emit_u16((uint16_t)dist);

        /* Patch the exit jump to land after the JMP_BACK we just emitted */
        size_t fwd_span = cs.here() - blk.fwd_patch_pos - 2;
        if (fwd_span > 0xFFFF) { cs.error("loop too large"); return; }
        cs.patch_u16(blk.fwd_patch_pos, (uint16_t)fwd_span);
        break;
    }
    }
}

/* ─────────────── Line dispatcher ──────────────────────────────────── */
static void parse_line(CS& cs, const Toks& t) {
    if (t.empty()) return;
    if (t[0].kind != TK::TK_IDENT) {
        cs.error("expected statement, got '" + t[0].text + "'"); return;
    }
    const std::string& kw = t[0].text;

    if (kw == "let")     { parse_let(cs, t);                    return; }
    if (kw == "copy")    { parse_copy(cs, t);                   return; }
    if (kw == "print")   { parse_print(cs, t, true);            return; }
    if (kw == "println") { parse_print(cs, t, false);           return; }
    if (kw == "input")   { parse_input(cs, t);                  return; }
    if (kw == "inc")     { parse_inc_dec(cs, t, OP_INC);        return; }
    if (kw == "dec")     { parse_inc_dec(cs, t, OP_DEC);        return; }
    if (kw == "not")     { parse_unary(cs, t, OP_NOT);          return; }
    if (kw == "neg")     { parse_unary(cs, t, OP_NEG);          return; }
    if (kw == "concat")  { parse_concat(cs, t);                 return; }
    if (kw == "len")     { parse_len(cs, t);                    return; }
    if (kw == "to_int")  { parse_to_int(cs, t);                 return; }
    if (kw == "to_str")  { parse_to_str(cs, t);                 return; }
    if (kw == "cmp")     { parse_cmp(cs, t);                    return; }
    if (kw == "and")     { parse_logical(cs, t, OP_AND);        return; }
    if (kw == "or")      { parse_logical(cs, t, OP_OR);         return; }
    if (kw == "if")      { parse_if(cs, t);                     return; }
    if (kw == "else")    { parse_else(cs, t);                   return; }
    if (kw == "end")     { parse_end(cs, t);                    return; }
    if (kw == "loop")    { parse_loop(cs, t);                   return; }

    for (int i = 0; ARITH_TABLE[i].kw; i++) {
        if (kw == ARITH_TABLE[i].kw) { parse_arith(cs, t, ARITH_TABLE[i]); return; }
    }

    cs.error("unknown statement '" + kw + "'"
             " — did you forget 'let', or misspell a keyword?");
}

/* ─────────────────── Public entry point ───────────────────────────── */
CompileResult compile(const std::string& source, const char* filename) {
    CompileResult result;

    CS cs;
    cs.filename = filename;

    std::istringstream stream(source);
    std::string raw_line;

    while (std::getline(stream, raw_line)) {
        cs.line++;
        std::vector<NsaLexer::Token> tokens;
        std::string lex_err;
        if (!NsaLexer::tokenize(raw_line, tokens, lex_err)) {
            fprintf(stderr, "%s:%d: error: %s\n", filename, cs.line, lex_err.c_str());
            cs.errors++;
            continue;
        }
        if (tokens.empty()) continue;
        parse_line(cs, tokens);
    }

    /* Check unclosed blocks */
    for (size_t i = 0; i < cs.blk_stack.size(); i++) {
        const char* kind_str = "if";
        if (cs.blk_stack[i].kind == BLK_ELSE)                                      kind_str = "else";
        if (cs.blk_stack[i].kind == BLK_LOOP_TIMES || cs.blk_stack[i].kind == BLK_LOOP_WHILE) kind_str = "loop";
        fprintf(stderr, "%s: error: unclosed '%s' block (missing 'end')\n", filename, kind_str);
        cs.errors++;
    }

    result.error_count   = cs.errors;
    result.warning_count = cs.warnings;

    if (cs.errors > 0) return result;

    cs.emit(OP_HALT);

    if (cs.bytecode.size() > 0xFFFF) {
        fprintf(stderr, "%s: error: compiled program too large (> 64 KB)\n", filename);
        return result;
    }

    result.ok        = true;
    result.bytecode  = cs.bytecode;
    result.sym_count = cs.next_id;
    return result;
}

} // namespace NsaCompiler