/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 danko1122q */

#include "nusa_compiler.h"
#include "nusa_lexer.h"
#include "nusa_opcodes.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include <sstream>

namespace NusaCompiler {

enum SymType { SYM_INT, SYM_STR };

struct Symbol {
    uint8_t id;
    SymType type;
};

enum BlockKind { BLK_IF, BLK_ELSE, BLK_LOOP_TIMES, BLK_LOOP_WHILE };

struct Block {
    BlockKind kind;
    size_t    fwd_patch_pos;
    bool      fwd_has_cmp;
    size_t    loop_start;
    uint8_t   counter_id;
    bool      owns_counter;
};

struct CS {
    std::map<std::string, Symbol> symbols;
    uint8_t                       next_id;
    std::vector<uint8_t>          bytecode;
    std::vector<Block>            blk_stack;
    int                           line;
    int                           errors;
    const char*                   filename;

    CS() : next_id(0), line(0), errors(0), filename("<input>") {}

    void emit(uint8_t b)      { bytecode.push_back(b); }

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

    bool lookup(const std::string& name, uint8_t& id_out, SymType* type_out = 0) {
        std::map<std::string,Symbol>::iterator it = symbols.find(name);
        if (it == symbols.end()) {
            error("undeclared variable '" + name + "'");
            return false;
        }
        id_out = it->second.id;
        if (type_out) *type_out = it->second.type;
        return true;
    }

    bool intern(const std::string& name, SymType type, uint8_t& id_out) {
        std::map<std::string,Symbol>::iterator it = symbols.find(name);
        if (it != symbols.end()) {
            if (it->second.type != type) {
                error("'" + name + "' was previously declared as "
                      + (it->second.type == SYM_INT ? "int" : "string"));
                return false;
            }
            id_out = it->second.id;
            return true;
        }
        if (next_id >= NUSA_MAX_VARS) {
            error("too many variables");
            return false;
        }
        id_out = next_id++;
        Symbol s; s.id = id_out; s.type = type;
        symbols[name] = s;
        return true;
    }

    bool alloc_internal(uint8_t& id_out) {
        if (next_id >= NUSA_MAX_VARS) { error("too many variables"); return false; }
        id_out = next_id++;
        return true;
    }
};

using Tok  = NusaLexer::Token;
using Toks = std::vector<Tok>;
using TK   = NusaLexer::TokenKind;

static bool is_ident(const Tok& t, const char* s) {
    return t.kind == TK::TK_IDENT && t.text == s;
}

static void parse_let(CS& cs, const Toks& t) {
    if (t.size() < 4) { cs.error("expected: let <name> = <value>"); return; }
    if (t[1].kind != TK::TK_IDENT) { cs.error("expected variable name after 'let'"); return; }
    std::string name = t[1].text;
    if (NusaLexer::is_keyword(name)) { cs.error("'" + name + "' is a keyword"); return; }
    if (t[2].kind != TK::TK_OP || t[2].text != "=") { cs.error("expected '=' after variable name"); return; }

    const Tok& val = t[3];
    if (val.kind == TK::TK_STRING) {
        if (val.text.size() > NUSA_MAX_STR_LEN) { cs.error("string too long"); return; }
        uint8_t id;
        if (!cs.intern(name, SYM_STR, id)) return;
        cs.emit(OP_SET_STR);
        cs.emit(id);
        cs.emit((uint8_t)val.text.size());
        for (size_t i = 0; i < val.text.size(); i++) cs.emit((uint8_t)val.text[i]);
    } else if (val.kind == TK::TK_INT) {
        uint8_t id;
        if (!cs.intern(name, SYM_INT, id)) return;
        cs.emit(OP_SET_INT);
        cs.emit(id);
        cs.emit_i32(val.ival);
    } else {
        cs.error("expected integer or string after '='");
    }
}

static void parse_print(CS& cs, const Toks& t) {
    if (t.size() < 2) { cs.error("expected argument after 'print'"); return; }
    const Tok& arg = t[1];
    if (arg.kind == TK::TK_STRING) {
        if (arg.text.size() > NUSA_MAX_STR_LEN) { cs.error("string too long"); return; }
        cs.emit(OP_PRINT_STR);
        cs.emit((uint8_t)arg.text.size());
        for (size_t i = 0; i < arg.text.size(); i++) cs.emit((uint8_t)arg.text[i]);
    } else if (arg.kind == TK::TK_IDENT && !NusaLexer::is_keyword(arg.text)) {
        uint8_t id;
        if (!cs.lookup(arg.text, id)) return;
        cs.emit(OP_PRINT_VAR);
        cs.emit(id);
    } else {
        cs.error("expected string literal or variable after 'print'");
    }
}

struct ArithDef { const char* kw; NusaOpcode op_imm; NusaOpcode op_var; };
static const ArithDef ARITH_TABLE[] = {
    {"add", OP_ADD_IMM, OP_ADD_VAR},
    {"sub", OP_SUB_IMM, OP_SUB_VAR},
    {"mul", OP_MUL_IMM, OP_MUL_VAR},
    {"div", OP_DIV_IMM, OP_DIV_VAR},
    {0, OP_NOP, OP_NOP}
};

static void parse_arith(CS& cs, const Toks& t, const ArithDef& def) {
    if (t.size() < 3) { cs.error(std::string(def.kw) + ": expected <var> <int|var>"); return; }
    if (t[1].kind != TK::TK_IDENT || NusaLexer::is_keyword(t[1].text)) {
        cs.error("expected destination variable"); return;
    }
    uint8_t dst_id; SymType dst_type;
    if (!cs.lookup(t[1].text, dst_id, &dst_type)) return;
    if (dst_type != SYM_INT) { cs.error("'" + t[1].text + "' is not an integer"); return; }

    const Tok& op = t[2];
    if (op.kind == TK::TK_INT) {
        if (def.op_imm == OP_DIV_IMM && op.ival == 0) { cs.error("division by zero"); return; }
        cs.emit(def.op_imm); cs.emit(dst_id); cs.emit_i32(op.ival);
    } else if (op.kind == TK::TK_IDENT && !NusaLexer::is_keyword(op.text)) {
        uint8_t src_id; SymType src_type;
        if (!cs.lookup(op.text, src_id, &src_type)) return;
        if (src_type != SYM_INT) { cs.error("'" + op.text + "' is not an integer"); return; }
        cs.emit(def.op_var); cs.emit(dst_id); cs.emit(src_id);
    } else {
        cs.error("expected integer or variable as operand");
    }
}

static void parse_if(CS& cs, const Toks& t) {
    if (t.size() < 5 || !is_ident(t[4], "then")) {
        cs.error("expected: if <var> <== | !=> <int> then"); return;
    }
    if (t[1].kind != TK::TK_IDENT || NusaLexer::is_keyword(t[1].text)) {
        cs.error("expected variable after 'if'"); return;
    }
    uint8_t id; SymType type;
    if (!cs.lookup(t[1].text, id, &type)) return;
    if (type != SYM_INT) { cs.error("'" + t[1].text + "' is not an integer"); return; }
    if (t[2].kind != TK::TK_OP || (t[2].text != "==" && t[2].text != "!=")) {
        cs.error("expected '==' or '!='"); return;
    }
    bool eq = (t[2].text == "==");
    if (t[3].kind != TK::TK_INT) { cs.error("expected integer after operator"); return; }
    int32_t cmp_val = t[3].ival;

    Block blk;
    blk.kind = BLK_IF;
    blk.loop_start = 0; blk.counter_id = 0; blk.owns_counter = false;
    blk.fwd_has_cmp = (cmp_val != 0);

    if (cmp_val == 0) {
        cs.emit(eq ? OP_JMP_IF_NZ : OP_JMP_IF_Z);
        cs.emit(id);
        blk.fwd_patch_pos = cs.here();
        cs.emit_u16(0);
    } else {
        cs.emit(eq ? OP_JMP_IF_NE : OP_JMP_IF_EQ);
        cs.emit(id);
        cs.emit_i32(cmp_val);
        blk.fwd_patch_pos = cs.here();
        cs.emit_u16(0);
    }
    cs.blk_stack.push_back(blk);
}

static void parse_else(CS& cs, const Toks&) {
    if (cs.blk_stack.empty() || cs.blk_stack.back().kind != BLK_IF) {
        cs.error("'else' without matching 'if'"); return;
    }
    Block& blk = cs.blk_stack.back();

    cs.emit(OP_JMP_FWD);
    size_t else_patch = cs.here();
    cs.emit_u16(0);

    size_t span = cs.here() - blk.fwd_patch_pos - 2;
    if (span > 0xFFFF) { cs.error("if-block too large"); return; }
    cs.patch_u16(blk.fwd_patch_pos, (uint16_t)span);

    blk.kind          = BLK_ELSE;
    blk.fwd_patch_pos = else_patch;
    blk.fwd_has_cmp   = false;
}

static void parse_loop(CS& cs, const Toks& t) {
    if (t.size() < 2) { cs.error("expected: 'loop <N> times' or 'loop while <var> <op> <val>'"); return; }

    Block blk;
    blk.fwd_patch_pos = 0; blk.fwd_has_cmp = false;
    blk.loop_start = 0; blk.counter_id = 0; blk.owns_counter = false;

    if (is_ident(t[1], "while")) {
        if (t.size() < 5) { cs.error("expected: loop while <var> <op> <int>"); return; }
        if (t[2].kind != TK::TK_IDENT || NusaLexer::is_keyword(t[2].text)) {
            cs.error("expected variable after 'while'"); return;
        }
        uint8_t id; SymType type;
        if (!cs.lookup(t[2].text, id, &type)) return;
        if (type != SYM_INT) { cs.error("loop variable must be integer"); return; }
        if (t[3].kind != TK::TK_OP || (t[3].text != "==" && t[3].text != "!=")) {
            cs.error("expected '==' or '!='"); return;
        }
        bool eq = (t[3].text == "==");
        if (t[4].kind != TK::TK_INT) { cs.error("expected integer after operator"); return; }
        int32_t cmp_val = t[4].ival;

        blk.kind = BLK_LOOP_WHILE;
        blk.fwd_has_cmp = (cmp_val != 0);
        blk.counter_id = id;
        blk.owns_counter = false;

        if (cmp_val == 0) {
            cs.emit(eq ? OP_JMP_IF_NZ : OP_JMP_IF_Z);
            cs.emit(id);
            blk.fwd_patch_pos = cs.here();
            cs.emit_u16(0);
        } else {
            cs.emit(eq ? OP_JMP_IF_NE : OP_JMP_IF_EQ);
            cs.emit(id);
            cs.emit_i32(cmp_val);
            blk.fwd_patch_pos = cs.here();
            cs.emit_u16(0);
        }
        size_t after_u16 = cs.here();
        size_t back = 2
                    + (blk.fwd_has_cmp ? 4 : 0)
                    + 1
                    + 1;
        blk.loop_start = after_u16 - back;
        cs.blk_stack.push_back(blk);
        return;
    }

    if (t[1].kind == TK::TK_INT) {
        if (t.size() < 3 || !is_ident(t[2], "times")) {
            cs.error("expected: loop <N> times"); return;
        }
        int32_t count = t[1].ival;
        if (count <= 0) { cs.error("loop count must be positive"); return; }

        uint8_t ctr;
        if (!cs.alloc_internal(ctr)) return;
        cs.emit(OP_SET_INT); cs.emit(ctr); cs.emit_i32(count);

        blk.kind         = BLK_LOOP_TIMES;
        blk.loop_start   = cs.here();
        blk.counter_id   = ctr;
        blk.owns_counter = true;
        cs.blk_stack.push_back(blk);
        return;
    }

    cs.error("expected: 'loop <N> times' or 'loop while <var> <op> <val>'");
}

static void parse_end(CS& cs, const Toks&) {
    if (cs.blk_stack.empty()) { cs.error("'end' without matching block"); return; }
    Block blk = cs.blk_stack.back();
    cs.blk_stack.pop_back();

    switch (blk.kind) {
    case BLK_IF:
    case BLK_ELSE: {
        size_t span = cs.here() - blk.fwd_patch_pos - 2;
        if (span > 0xFFFF) { cs.error("block too large"); return; }
        cs.patch_u16(blk.fwd_patch_pos, (uint16_t)span);
        break;
    }
    case BLK_LOOP_TIMES: {
        cs.emit(OP_SUB_IMM); cs.emit(blk.counter_id); cs.emit_i32(1);
        size_t after_instr = cs.here() + 1 + 1 + 2;
        if (after_instr < blk.loop_start) { cs.error("internal: bad loop start"); return; }
        size_t dist = after_instr - blk.loop_start;
        if (dist > 0xFFFF) { cs.error("loop body too large"); return; }
        cs.emit(OP_JMP_BACK_NZ);
        cs.emit(blk.counter_id);
        cs.emit_u16((uint16_t)dist);
        break;
    }
    case BLK_LOOP_WHILE: {
        size_t after_instr = cs.here() + 1 + 2;
        if (after_instr < blk.loop_start) { cs.error("internal: bad loop start"); return; }
        size_t dist = after_instr - blk.loop_start;
        if (dist > 0xFFFF) { cs.error("loop body too large"); return; }
        cs.emit(OP_JMP_BACK);
        cs.emit_u16((uint16_t)dist);

        size_t fwd_span = cs.here() - blk.fwd_patch_pos - 2;
        if (fwd_span > 0xFFFF) { cs.error("loop too large"); return; }
        cs.patch_u16(blk.fwd_patch_pos, (uint16_t)fwd_span);
        break;
    }
    }
}

static void parse_line(CS& cs, const std::vector<NusaLexer::Token>& t) {
    if (t.empty()) return;
    if (t[0].kind != TK::TK_IDENT) {
        cs.error("expected statement keyword, got '" + t[0].text + "'"); return;
    }
    const std::string& kw = t[0].text;

    if (kw == "let")   { parse_let(cs, t);   return; }
    if (kw == "print") { parse_print(cs, t); return; }
    if (kw == "if")    { parse_if(cs, t);    return; }
    if (kw == "else")  { parse_else(cs, t);  return; }
    if (kw == "end")   { parse_end(cs, t);   return; }
    if (kw == "loop")  { parse_loop(cs, t);  return; }

    for (int i = 0; ARITH_TABLE[i].kw; i++) {
        if (kw == ARITH_TABLE[i].kw) {
            parse_arith(cs, t, ARITH_TABLE[i]); return;
        }
    }

    cs.error("unknown statement '" + kw + "'");
}

CompileResult compile(const std::string& source, const char* filename) {
    CompileResult result;
    result.ok = false; result.error_count = 0; result.sym_count = 0;

    CS cs;
    cs.filename = filename;

    std::istringstream stream(source);
    std::string raw_line;

    while (std::getline(stream, raw_line)) {
        cs.line++;
        std::vector<NusaLexer::Token> tokens;
        std::string lex_err;
        if (!NusaLexer::tokenize(raw_line, tokens, lex_err)) {
            fprintf(stderr, "%s:%d: error: %s\n", filename, cs.line, lex_err.c_str());
            cs.errors++;
            continue;
        }
        if (tokens.empty()) continue;
        parse_line(cs, tokens);
    }

    for (size_t i = 0; i < cs.blk_stack.size(); i++) {
        const char* kind_str = "if";
        if (cs.blk_stack[i].kind == BLK_ELSE)       kind_str = "else";
        if (cs.blk_stack[i].kind == BLK_LOOP_TIMES ||
            cs.blk_stack[i].kind == BLK_LOOP_WHILE) kind_str = "loop";
        fprintf(stderr, "%s: error: unclosed '%s' (missing 'end')\n", filename, kind_str);
        cs.errors++;
    }

    if (cs.errors > 0) {
        result.error_count = cs.errors;
        return result;
    }

    cs.emit(OP_HALT);

    if (cs.bytecode.size() > 0xFFFF) {
        fprintf(stderr, "%s: error: program too large\n", filename);
        return result;
    }

    result.ok          = true;
    result.bytecode    = cs.bytecode;
    result.sym_count   = cs.next_id;
    return result;
}

} // namespace NusaCompiler