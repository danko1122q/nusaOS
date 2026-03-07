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

/* ── Symbol types ────────────────────────────────────────────────────── */
enum SymType { SYM_INT, SYM_STR, SYM_BOOL, SYM_ARRAY };
struct Symbol {
    uint8_t id;
    SymType type;
    bool    is_internal;
    /* Array-specific: element type + declared size */
    SymType elem_type;
    uint8_t arr_size;
};

/* ── Function descriptor ─────────────────────────────────────────────── */
struct ParamDef { std::string name; SymType type; };
struct FuncDef {
    std::string            name;
    std::vector<ParamDef>  params;
    bool                   has_ret;
    std::string            ret_name;
    SymType                ret_type;
    size_t                 addr;
    FuncDef() : has_ret(false), ret_type(SYM_INT), addr(0) {}
};

/* ── Block / control-flow stack ──────────────────────────────────────── */
enum BlockKind { BLK_IF, BLK_ELSE, BLK_LOOP_TIMES, BLK_LOOP_WHILE, BLK_FUNC };
struct Block {
    BlockKind kind;
    size_t    fwd_patch_pos;
    size_t    loop_start;
    uint8_t   counter_id;
    bool      owns_counter;
};

/* ── Compiler state ──────────────────────────────────────────────────── */
struct CS {
    std::map<std::string, Symbol>   globals;
    uint8_t next_global = 0;

    std::map<std::string, Symbol>   locals;
    uint8_t next_local  = 0;
    bool    in_func     = false;
    std::string cur_func_name;

    std::vector<uint8_t>            bytecode;
    std::vector<Block>              blk_stack;
    std::map<std::string, FuncDef>  funcs;
    std::vector<std::pair<size_t,std::string>> call_patches;

    int         line     = 0;
    int         errors   = 0;
    int         warnings = 0;
    const char* filename = "<input>";

    void emit(uint8_t b)         { bytecode.push_back(b); }
    void emit_u16(uint16_t v)    { bytecode.push_back(v&0xFF); bytecode.push_back((v>>8)&0xFF); }
    void emit_i32(int32_t v)     {
        bytecode.push_back(v&0xFF); bytecode.push_back((v>>8)&0xFF);
        bytecode.push_back((v>>16)&0xFF); bytecode.push_back((v>>24)&0xFF);
    }
    void patch_u16(size_t pos, uint16_t v) {
        bytecode[pos]=(v&0xFF); bytecode[pos+1]=((v>>8)&0xFF);
    }
    size_t here() const { return bytecode.size(); }

    void error(const std::string& msg) {
        fprintf(stderr,"%s:%d: error: %s\n",filename,line,msg.c_str()); errors++;
    }
    void warn(const std::string& msg) {
        fprintf(stderr,"%s:%d: warning: %s\n",filename,line,msg.c_str()); warnings++;
    }

    /* Lookup variable — locals first (when in func), then globals */
    bool lookup(const std::string& name, uint8_t& id_out, SymType* type_out=nullptr,
                Symbol** sym_out=nullptr) {
        if (in_func) {
            auto it=locals.find(name);
            if (it!=locals.end()) {
                id_out=it->second.id;
                if (type_out) *type_out=it->second.type;
                if (sym_out)  *sym_out =&it->second;
                return true;
            }
        }
        auto it=globals.find(name);
        if (it==globals.end()) { error("undeclared variable '"+name+"'"); return false; }
        id_out=it->second.id;
        if (type_out) *type_out=it->second.type;
        if (sym_out)  *sym_out =&it->second;
        return true;
    }

    /* Intern variable in current scope */
    bool intern(const std::string& name, SymType type, uint8_t& id_out,
                Symbol** sym_out=nullptr) {
        if (in_func) {
            auto it=locals.find(name);
            if (it!=locals.end()) {
                if (it->second.type!=type) {
                    error("'"+name+"' was previously declared as "+sym_type_name(it->second.type));
                    return false;
                }
                id_out=it->second.id;
                if (sym_out) *sym_out=&it->second;
                return true;
            }
            if (next_local>=NSA_MAX_LOCALS) { error("too many local variables (max 64)"); return false; }
            id_out=next_local++;
            Symbol s{}; s.id=id_out; s.type=type; s.is_internal=false;
            s.elem_type=SYM_INT; s.arr_size=0;
            locals[name]=s;
            if (sym_out) *sym_out=&locals[name];
            return true;
        }
        auto it=globals.find(name);
        if (it!=globals.end()) {
            if (it->second.type!=type) {
                error("'"+name+"' was previously declared as "+sym_type_name(it->second.type));
                return false;
            }
            id_out=it->second.id;
            if (sym_out) *sym_out=&it->second;
            return true;
        }
        if (next_global>=NSA_MAX_VARS) { error("too many variables (max 200)"); return false; }
        id_out=next_global++;
        Symbol s{}; s.id=id_out; s.type=type; s.is_internal=false;
        s.elem_type=SYM_INT; s.arr_size=0;
        globals[name]=s;
        if (sym_out) *sym_out=&globals[name];
        return true;
    }

    /* Allocate N consecutive global slots (used by array declarations) */
    bool alloc_slots(uint8_t count, uint8_t& first_id) {
        if (in_func) {
            /* Arrays inside functions — allocate from local pool */
            if ((int)next_local + count > NSA_MAX_LOCALS) {
                error("not enough local slots for array (need "+std::to_string(count)+")");
                return false;
            }
            first_id = next_local;
            next_local = (uint8_t)(next_local + count);
            return true;
        }
        if ((int)next_global + count > NSA_MAX_VARS) {
            error("not enough variable slots for array (need "+std::to_string(count)+")");
            return false;
        }
        first_id = next_global;
        next_global = (uint8_t)(next_global + count);
        return true;
    }

    bool alloc_temp(uint8_t& id_out) {
        if (in_func) {
            if (next_local>=NSA_MAX_LOCALS) { error("too many locals"); return false; }
            id_out=next_local++; return true;
        }
        if (next_global>=NSA_MAX_VARS) { error("too many variables"); return false; }
        id_out=next_global++; return true;
    }

    static const char* sym_type_name(SymType t) {
        switch(t) {
            case SYM_INT:   return "int";
            case SYM_STR:   return "string";
            case SYM_BOOL:  return "bool";
            case SYM_ARRAY: return "array";
        }
        return "?";
    }
};

using Tok  = NsaLexer::Token;
using Toks = std::vector<Tok>;
using TK   = NsaLexer::TokenKind;

static bool is_ident(const Tok& t, const char* s) {
    return t.kind==TK::TK_IDENT && t.text==s;
}
static void emit_str_lit(CS& cs, const std::string& s) {
    cs.emit((uint8_t)s.size());
    for (char c : s) cs.emit((uint8_t)c);
}

/* ─────────────────────── Basic statement parsers ──────────────────── */

static void parse_let(CS& cs, const Toks& t) {
    if (t.size()<4) { cs.error("expected: let <name> = <value>"); return; }
    if (t[1].kind!=TK::TK_IDENT) { cs.error("expected variable name after 'let'"); return; }
    const std::string& name=t[1].text;
    if (NsaLexer::is_keyword(name)) { cs.error("'"+name+"' is a reserved keyword"); return; }
    if (t[2].kind!=TK::TK_OP||t[2].text!="=") { cs.error("expected '='"); return; }
    const Tok& val=t[3];
    if (val.kind==TK::TK_BOOL) {
        uint8_t id; if (!cs.intern(name,SYM_BOOL,id)) return;
        cs.emit(OP_SET_BOOL); cs.emit(id); cs.emit((uint8_t)val.ival); return;
    }
    if (val.kind==TK::TK_STRING) {
        if (val.text.size()>NSA_MAX_STR_LEN) { cs.error("string literal too long"); return; }
        uint8_t id; if (!cs.intern(name,SYM_STR,id)) return;
        cs.emit(OP_SET_STR); cs.emit(id); emit_str_lit(cs,val.text); return;
    }
    if (val.kind==TK::TK_INT) {
        uint8_t id; if (!cs.intern(name,SYM_INT,id)) return;
        cs.emit(OP_SET_INT); cs.emit(id); cs.emit_i32(val.ival); return;
    }
    if (val.kind==TK::TK_IDENT && !NsaLexer::is_keyword(val.text)) {
        uint8_t src; SymType st; if (!cs.lookup(val.text,src,&st)) return;
        uint8_t dst; if (!cs.intern(name,st,dst)) return;
        cs.emit(OP_COPY); cs.emit(dst); cs.emit(src); return;
    }
    cs.error("expected integer, string, bool, or variable after '='");
}

static void parse_copy(CS& cs, const Toks& t) {
    if (t.size()<3) { cs.error("expected: copy <dst> <src>"); return; }
    uint8_t src; SymType st; if (!cs.lookup(t[2].text,src,&st)) return;
    uint8_t dst; if (!cs.intern(t[1].text,st,dst)) return;
    cs.emit(OP_COPY); cs.emit(dst); cs.emit(src);
}

static void parse_print(CS& cs, const Toks& t, bool nl) {
    if (t.size()<2) { cs.error("expected argument after 'print'"); return; }
    const Tok& arg=t[1];
    NsaOpcode sop = nl ? OP_PRINT_STR    : OP_PRINT_STR_NL;
    NsaOpcode vop = nl ? OP_PRINT_VAR    : OP_PRINT_VAR_NL;
    if (arg.kind==TK::TK_STRING) {
        if (arg.text.size()>NSA_MAX_STR_LEN) { cs.error("string literal too long"); return; }
        cs.emit(sop); emit_str_lit(cs,arg.text); return;
    }
    if (arg.kind==TK::TK_IDENT && !NsaLexer::is_keyword(arg.text)) {
        uint8_t id; if (!cs.lookup(arg.text,id)) return;
        cs.emit(vop); cs.emit(id); return;
    }
    cs.error("expected string literal or variable after 'print'");
}

static void parse_input(CS& cs, const Toks& t) {
    if (t.size()<2 || t[1].kind!=TK::TK_IDENT || NsaLexer::is_keyword(t[1].text)) {
        cs.error("expected: input <variable>"); return;
    }
    const std::string& name=t[1].text;
    Symbol* sym=nullptr;
    if (cs.in_func) { auto it=cs.locals.find(name); if(it!=cs.locals.end()) sym=&it->second; }
    if (!sym)       { auto it=cs.globals.find(name); if(it!=cs.globals.end()) sym=&it->second; }
    if (!sym) {
        uint8_t id; if (!cs.intern(name,SYM_STR,id)) return;
        cs.emit(OP_INPUT_STR); cs.emit(id); return;
    }
    switch (sym->type) {
        case SYM_INT:  cs.emit(OP_INPUT_INT); cs.emit(sym->id); break;
        case SYM_STR:  cs.emit(OP_INPUT_STR); cs.emit(sym->id); break;
        case SYM_BOOL: cs.error("'input' into a bool variable is not supported"); break;
        case SYM_ARRAY:cs.error("'input' into an array is not supported — use aget/aset"); break;
    }
}

static void parse_inc_dec(CS& cs, const Toks& t, NsaOpcode op) {
    if (t.size()<2||t[1].kind!=TK::TK_IDENT||NsaLexer::is_keyword(t[1].text)) {
        cs.error("expected: inc/dec <variable>"); return;
    }
    uint8_t id; SymType type; if (!cs.lookup(t[1].text,id,&type)) return;
    if (type!=SYM_INT) { cs.error("'"+t[1].text+"' is not an integer"); return; }
    cs.emit(op); cs.emit(id);
}

static void parse_unary(CS& cs, const Toks& t, NsaOpcode op) {
    if (t.size()<2||t[1].kind!=TK::TK_IDENT||NsaLexer::is_keyword(t[1].text)) {
        cs.error("expected variable after unary op"); return;
    }
    uint8_t id; SymType type; if (!cs.lookup(t[1].text,id,&type)) return;
    if (op==OP_NEG && type!=SYM_INT)  { cs.error("'neg' requires integer"); return; }
    if (op==OP_NOT && type!=SYM_INT && type!=SYM_BOOL) {
        cs.error("'not' requires int or bool"); return;
    }
    cs.emit(op); cs.emit(id);
}

struct ArithDef { const char* kw; NsaOpcode op_imm; NsaOpcode op_var; };
static const ArithDef ARITH_TABLE[] = {
    {"add",OP_ADD_IMM,OP_ADD_VAR}, {"sub",OP_SUB_IMM,OP_SUB_VAR},
    {"mul",OP_MUL_IMM,OP_MUL_VAR}, {"div",OP_DIV_IMM,OP_DIV_VAR},
    {"mod",OP_MOD_IMM,OP_MOD_VAR}, {nullptr,OP_NOP,OP_NOP}
};

static void parse_arith(CS& cs, const Toks& t, const ArithDef& def) {
    if (t.size()<3) { cs.error(std::string(def.kw)+": expected <var> <int|var>"); return; }
    uint8_t dst; SymType dt; if (!cs.lookup(t[1].text,dst,&dt)) return;
    if (dt!=SYM_INT) { cs.error("'"+t[1].text+"' is not an integer"); return; }
    if (t[2].kind==TK::TK_INT) {
        if ((def.op_imm==OP_DIV_IMM||def.op_imm==OP_MOD_IMM)&&t[2].ival==0) {
            cs.error("division/modulo by zero"); return;
        }
        cs.emit(def.op_imm); cs.emit(dst); cs.emit_i32(t[2].ival);
    } else if (t[2].kind==TK::TK_IDENT && !NsaLexer::is_keyword(t[2].text)) {
        uint8_t src; SymType st; if (!cs.lookup(t[2].text,src,&st)) return;
        if (st!=SYM_INT) { cs.error("'"+t[2].text+"' is not an integer"); return; }
        cs.emit(def.op_var); cs.emit(dst); cs.emit(src);
    } else { cs.error("expected integer literal or variable as operand"); }
}

static void parse_concat(CS& cs, const Toks& t) {
    if (t.size()<3) { cs.error("expected: concat <dst> <src|\"lit\">"); return; }
    uint8_t dst; SymType dt; if (!cs.lookup(t[1].text,dst,&dt)) return;
    if (dt!=SYM_STR) { cs.error("'"+t[1].text+"' is not a string"); return; }
    if (t[2].kind==TK::TK_STRING) {
        cs.emit(OP_CONCAT_LIT); cs.emit(dst); emit_str_lit(cs,t[2].text);
    } else if (t[2].kind==TK::TK_IDENT && !NsaLexer::is_keyword(t[2].text)) {
        uint8_t src; SymType st; if (!cs.lookup(t[2].text,src,&st)) return;
        if (st!=SYM_STR) { cs.error("'"+t[2].text+"' is not a string"); return; }
        cs.emit(OP_CONCAT); cs.emit(dst); cs.emit(src);
    } else { cs.error("expected string variable or literal"); }
}

static void parse_len(CS& cs, const Toks& t) {
    if (t.size()<3) { cs.error("expected: len <int_var> <str_var>"); return; }
    uint8_t dst;
    Symbol* existing=nullptr;
    if (cs.in_func) { auto it=cs.locals.find(t[1].text); if(it!=cs.locals.end()) existing=&it->second; }
    if (!existing)  { auto it=cs.globals.find(t[1].text); if(it!=cs.globals.end()) existing=&it->second; }
    if (!existing) { if (!cs.intern(t[1].text,SYM_INT,dst)) return; }
    else { if (existing->type!=SYM_INT) { cs.error("'"+t[1].text+"' is not an integer"); return; } dst=existing->id; }
    uint8_t src; SymType st; if (!cs.lookup(t[2].text,src,&st)) return;
    if (st!=SYM_STR) { cs.error("'"+t[2].text+"' is not a string"); return; }
    cs.emit(OP_LEN); cs.emit(dst); cs.emit(src);
}

static void parse_to_int(CS& cs, const Toks& t) {
    if (t.size()<3) { cs.error("expected: to_int <int_var> <str_var>"); return; }
    uint8_t dst; if (!cs.intern(t[1].text,SYM_INT,dst)) return;
    uint8_t src; SymType st; if (!cs.lookup(t[2].text,src,&st)) return;
    if (st!=SYM_STR) { cs.error("'"+t[2].text+"' is not a string"); return; }
    cs.emit(OP_STR_TO_INT); cs.emit(dst); cs.emit(src);
}

static void parse_to_str(CS& cs, const Toks& t) {
    if (t.size()<3) { cs.error("expected: to_str <str_var> <int_var>"); return; }
    uint8_t dst; if (!cs.intern(t[1].text,SYM_STR,dst)) return;
    uint8_t src; SymType st; if (!cs.lookup(t[2].text,src,&st)) return;
    if (st!=SYM_INT) { cs.error("'"+t[2].text+"' is not an integer"); return; }
    cs.emit(OP_INT_TO_STR); cs.emit(dst); cs.emit(src);
}

/* ─────────────────────── cmp — integer AND string comparison ───────── */
static void parse_cmp(CS& cs, const Toks& t) {
    /* Syntax:
     *   cmp <dst_bool> <a> <op> <b>    — integer comparison (unchanged)
     *   cmp <dst_bool> <a> == <b>      — string comparison (a,b are strings)
     *   cmp <dst_bool> <a> != <b>      — string comparison
     */
    if (t.size()<5) { cs.error("expected: cmp <bool_var> <a> <op> <b>"); return; }
    uint8_t dst; if (!cs.intern(t[1].text,SYM_BOOL,dst)) return;

    uint8_t va; SymType vta; if (!cs.lookup(t[2].text,va,&vta)) return;
    if (t[3].kind!=TK::TK_OP) { cs.error("expected comparison operator"); return; }
    const std::string& op=t[3].text;
    uint8_t vb; SymType vtb; if (!cs.lookup(t[4].text,vb,&vtb)) return;

    /* String comparison — only == and != */
    if (vta==SYM_STR || vtb==SYM_STR) {
        if (vta!=SYM_STR||vtb!=SYM_STR) { cs.error("cmp: both operands must be string for string comparison"); return; }
        if (op=="==") { cs.emit(OP_SCMP_EQ); cs.emit(dst); cs.emit(va); cs.emit(vb); return; }
        if (op=="!=") { cs.emit(OP_SCMP_NE); cs.emit(dst); cs.emit(va); cs.emit(vb); return; }
        cs.error("string comparison only supports == and !=");
        return;
    }

    /* Integer comparison */
    if (vta!=SYM_INT) { cs.error("comparison operands must be integer"); return; }
    if (vtb!=SYM_INT) { cs.error("comparison operands must be integer"); return; }
    NsaOpcode cop;
    if      (op=="==") cop=OP_CMP_EQ;
    else if (op=="!=") cop=OP_CMP_NE;
    else if (op=="<")  cop=OP_CMP_LT;
    else if (op==">")  cop=OP_CMP_GT;
    else if (op=="<=") cop=OP_CMP_LE;
    else if (op==">=") cop=OP_CMP_GE;
    else { cs.error("unknown comparison operator '"+op+"'"); return; }
    cs.emit(cop); cs.emit(dst); cs.emit(va); cs.emit(vb);
}

static void parse_logical(CS& cs, const Toks& t, NsaOpcode op) {
    if (t.size()<4) { cs.error("expected: and/or <dst> <a> <b>"); return; }
    uint8_t dst; if (!cs.intern(t[1].text,SYM_BOOL,dst)) return;
    uint8_t va; if (!cs.lookup(t[2].text,va)) return;
    uint8_t vb; if (!cs.lookup(t[3].text,vb)) return;
    cs.emit(op); cs.emit(dst); cs.emit(va); cs.emit(vb);
}

/* ─────────────────────── Arrays ─────────────────────────────────────
 *
 * Declaration:
 *   arr int  scores 5       — int array, 5 elements, all init to 0
 *   arr str  names  3       — string array, 3 elements, all ""
 *   arr bool flags  4       — bool array
 *
 * Access:
 *   aget dst  arr_name idx  — load element[idx] into dst
 *   aset arr_name idx  val  — store val into element[idx]
 *   aset arr_name idx  42   — store literal int
 *   aset arr_name idx  "x"  — store literal string
 *   alen dst  arr_name      — store declared size into dst (int)
 *
 * Arrays are NOT supported inside functions (local scope) in v2.2.
 * They are global-only to keep slot accounting simple.
 * ------------------------------------------------------------------ */

static void parse_arr(CS& cs, const Toks& t) {
    /* arr <type> <name> <size> */
    if (t.size()<4) { cs.error("expected: arr <int|str|bool> <name> <size>"); return; }

    if (cs.in_func) { cs.error("arrays are not supported inside functions in v2.2"); return; }

    /* type token */
    if (t[1].kind!=TK::TK_IDENT) { cs.error("expected type: int, str, or bool"); return; }
    SymType etype;
    if      (t[1].text=="int")  etype=SYM_INT;
    else if (t[1].text=="str")  etype=SYM_STR;
    else if (t[1].text=="bool") etype=SYM_BOOL;
    else { cs.error("unknown array type '"+t[1].text+"' — use int, str, or bool"); return; }

    /* name */
    if (t[2].kind!=TK::TK_IDENT || NsaLexer::is_keyword(t[2].text)) {
        cs.error("expected array name"); return;
    }
    const std::string& name=t[2].text;
    if (cs.globals.count(name)) { cs.error("'"+name+"' is already declared"); return; }

    /* size */
    if (t[3].kind!=TK::TK_INT || t[3].ival<=0) {
        cs.error("array size must be a positive integer literal"); return;
    }
    int sz=t[3].ival;
    if (sz>NSA_MAX_ARRAY_SIZE) {
        cs.error("array size "+std::to_string(sz)+" exceeds maximum ("+
                 std::to_string(NSA_MAX_ARRAY_SIZE)+")"); return;
    }

    /* Allocate base_id (descriptor) + sz element slots */
    uint8_t base_id;
    if (!cs.alloc_slots((uint8_t)(sz+1), base_id)) return;

    /* Register the array in the symbol table */
    Symbol s{}; s.id=base_id; s.type=SYM_ARRAY; s.is_internal=false;
    s.elem_type=etype; s.arr_size=(uint8_t)sz;
    cs.globals[name]=s;

    /* Emit: SET_INT base_id sz  (stores count in slot 0 so ARR_LEN works) */
    cs.emit(OP_SET_INT); cs.emit(base_id); cs.emit_i32(sz);

    /* Emit initializers for each element slot */
    for (int i=0;i<sz;i++) {
        uint8_t slot=(uint8_t)(base_id+1+i);
        switch(etype) {
            case SYM_INT:
                cs.emit(OP_SET_INT); cs.emit(slot); cs.emit_i32(0);
                break;
            case SYM_STR:
                cs.emit(OP_SET_STR); cs.emit(slot); emit_str_lit(cs,"");
                break;
            case SYM_BOOL:
                cs.emit(OP_SET_BOOL); cs.emit(slot); cs.emit(0);
                break;
            default: break;
        }
    }
}

/* aget <dst> <arr_name> <idx_var|int_literal> */
static void parse_aget(CS& cs, const Toks& t) {
    if (t.size()<4) { cs.error("expected: aget <dst> <array> <idx>"); return; }

    /* destination */
    const std::string& dst_name=t[1].text;
    const std::string& arr_name=t[2].text;

    /* look up array */
    auto ait = cs.globals.find(arr_name);
    if (ait==cs.globals.end() || ait->second.type!=SYM_ARRAY) {
        cs.error("'"+arr_name+"' is not a declared array"); return;
    }
    Symbol& arr=ait->second;

    /* intern destination with element type */
    uint8_t dst; if (!cs.intern(dst_name, arr.elem_type, dst)) return;

    /* index: variable or literal */
    if (t[3].kind==TK::TK_INT) {
        /* constant index — emit literal into a temp int, then ARR_GET */
        int idx=t[3].ival;
        if (idx<0||idx>=(int)arr.arr_size) {
            cs.error("array index "+std::to_string(idx)+" out of bounds (size "+
                     std::to_string(arr.arr_size)+")"); return;
        }
        uint8_t tmp; if (!cs.alloc_temp(tmp)) return;
        cs.emit(OP_SET_INT); cs.emit(tmp); cs.emit_i32(idx);
        cs.emit(OP_ARR_GET); cs.emit(dst); cs.emit(arr.id); cs.emit(tmp);
    } else if (t[3].kind==TK::TK_IDENT && !NsaLexer::is_keyword(t[3].text)) {
        uint8_t idx_id; SymType it2; if (!cs.lookup(t[3].text,idx_id,&it2)) return;
        if (it2!=SYM_INT) { cs.error("array index must be an integer"); return; }
        cs.emit(OP_ARR_GET); cs.emit(dst); cs.emit(arr.id); cs.emit(idx_id);
    } else { cs.error("expected integer or variable as array index"); }
}

/* aset <arr_name> <idx_var|literal> <src_var|literal> */
static void parse_aset(CS& cs, const Toks& t) {
    if (t.size()<4) { cs.error("expected: aset <array> <idx> <value>"); return; }

    const std::string& arr_name=t[1].text;
    auto ait=cs.globals.find(arr_name);
    if (ait==cs.globals.end() || ait->second.type!=SYM_ARRAY) {
        cs.error("'"+arr_name+"' is not a declared array"); return;
    }
    Symbol& arr=ait->second;

    /* Resolve index into a var slot */
    uint8_t idx_id;
    if (t[2].kind==TK::TK_INT) {
        int idx=t[2].ival;
        if (idx<0||idx>=(int)arr.arr_size) {
            cs.error("array index "+std::to_string(idx)+" out of bounds (size "+
                     std::to_string(arr.arr_size)+")"); return;
        }
        if (!cs.alloc_temp(idx_id)) return;
        cs.emit(OP_SET_INT); cs.emit(idx_id); cs.emit_i32(idx);
    } else if (t[2].kind==TK::TK_IDENT && !NsaLexer::is_keyword(t[2].text)) {
        SymType it2; if (!cs.lookup(t[2].text,idx_id,&it2)) return;
        if (it2!=SYM_INT) { cs.error("array index must be an integer"); return; }
    } else { cs.error("expected integer or variable as array index"); return; }

    /* Value: variable or literal */
    const Tok& val=t[3];
    if (val.kind==TK::TK_IDENT && !NsaLexer::is_keyword(val.text)) {
        /* src variable */
        uint8_t src; SymType st; if (!cs.lookup(val.text,src,&st)) return;
        cs.emit(OP_ARR_SET); cs.emit(arr.id); cs.emit(idx_id); cs.emit(src);
    } else {
        /* immediate literal */
        if (val.kind==TK::TK_INT) {
            if (arr.elem_type!=SYM_INT) { cs.error("array element type is not int"); return; }
            cs.emit(OP_ARR_SET_IMM); cs.emit(arr.id); cs.emit(idx_id);
            cs.emit(0x01); cs.emit_i32(val.ival);
        } else if (val.kind==TK::TK_STRING) {
            if (arr.elem_type!=SYM_STR) { cs.error("array element type is not string"); return; }
            if (val.text.size()>NSA_MAX_STR_LEN) { cs.error("string literal too long"); return; }
            cs.emit(OP_ARR_SET_IMM); cs.emit(arr.id); cs.emit(idx_id);
            cs.emit(0x02); emit_str_lit(cs,val.text);
        } else if (val.kind==TK::TK_BOOL) {
            if (arr.elem_type!=SYM_BOOL) { cs.error("array element type is not bool"); return; }
            cs.emit(OP_ARR_SET_IMM); cs.emit(arr.id); cs.emit(idx_id);
            cs.emit(0x03); cs.emit((uint8_t)val.ival);
        } else { cs.error("expected variable or literal as value"); }
    }
}

/* alen <dst_int> <arr_name> */
static void parse_alen(CS& cs, const Toks& t) {
    if (t.size()<3) { cs.error("expected: alen <int_var> <array>"); return; }
    const std::string& arr_name=t[2].text;
    auto ait=cs.globals.find(arr_name);
    if (ait==cs.globals.end() || ait->second.type!=SYM_ARRAY) {
        cs.error("'"+arr_name+"' is not a declared array"); return;
    }
    uint8_t dst;
    Symbol* existing=nullptr;
    { auto it=cs.globals.find(t[1].text); if(it!=cs.globals.end()) existing=&it->second; }
    if (!existing) { if (!cs.intern(t[1].text,SYM_INT,dst)) return; }
    else { if(existing->type!=SYM_INT){cs.error("'"+t[1].text+"' is not an integer");return;} dst=existing->id; }
    cs.emit(OP_ARR_LEN); cs.emit(dst); cs.emit(ait->second.id);
}

/* ─────────────────────── Control flow ───────────────────────────── */

static bool emit_condition_jump(CS& cs, const Toks& t, size_t cs_, size_t ce_,
                                bool jit, Block& blk) {
    if (ce_-cs_==1) {
        if (t[cs_].kind!=TK::TK_IDENT||NsaLexer::is_keyword(t[cs_].text)) {
            cs.error("expected variable in condition"); return false;
        }
        uint8_t id; if (!cs.lookup(t[cs_].text,id)) return false;
        cs.emit(jit?OP_JMP_IF_TRUE:OP_JMP_IF_FALSE);
        cs.emit(id); blk.fwd_patch_pos=cs.here(); cs.emit_u16(0); return true;
    }
    if (ce_-cs_==3) {
        uint8_t id; SymType type;
        if (!cs.lookup(t[cs_].text,id,&type)) return false;
        if (type!=SYM_INT) { cs.error("comparison condition requires integer variable"); return false; }
        if (t[cs_+1].kind!=TK::TK_OP) { cs.error("expected comparison operator"); return false; }
        const std::string& op=t[cs_+1].text;
        if (t[cs_+2].kind!=TK::TK_INT) { cs.error("expected integer literal"); return false; }
        int32_t cmp=t[cs_+2].ival;
        NsaOpcode jop;
        if      (op=="==") jop=OP_JMP_IF_NE; else if(op=="!=") jop=OP_JMP_IF_EQ;
        else if (op=="<")  jop=OP_JMP_IF_GE; else if(op==">")  jop=OP_JMP_IF_LE;
        else if (op=="<=") jop=OP_JMP_IF_GT; else if(op==">=") jop=OP_JMP_IF_LT;
        else { cs.error("unknown operator '"+op+"'"); return false; }
        cs.emit(jop); cs.emit(id); cs.emit_i32(cmp);
        blk.fwd_patch_pos=cs.here(); cs.emit_u16(0); return true;
    }
    cs.error("malformed condition"); return false;
}

static void parse_if(CS& cs, const Toks& t) {
    size_t tp=0;
    for (size_t i=1;i<t.size();i++) if(is_ident(t[i],"then")){tp=i;break;}
    if (tp==0) { cs.error("expected 'then' in 'if' statement"); return; }
    Block blk{}; blk.kind=BLK_IF;
    if (!emit_condition_jump(cs,t,1,tp,false,blk)) return;
    cs.blk_stack.push_back(blk);
}

static void parse_else(CS& cs, const Toks&) {
    if (cs.blk_stack.empty()||cs.blk_stack.back().kind!=BLK_IF) {
        cs.error("'else' without matching 'if'"); return;
    }
    Block& blk=cs.blk_stack.back();
    cs.emit(OP_JMP_FWD); size_t ep=cs.here(); cs.emit_u16(0);
    size_t span=cs.here()-blk.fwd_patch_pos-2;
    if (span>0xFFFF) { cs.error("if-block too large"); return; }
    cs.patch_u16(blk.fwd_patch_pos,(uint16_t)span);
    blk.kind=BLK_ELSE; blk.fwd_patch_pos=ep;
}

static void parse_loop(CS& cs, const Toks& t) {
    if (t.size()<2) { cs.error("expected: 'loop <N> times' or 'loop while <cond>'"); return; }
    Block blk{}; blk.fwd_patch_pos=0;
    if (is_ident(t[1],"while")) {
        if (t.size()<3) { cs.error("expected condition after 'loop while'"); return; }
        blk.kind=BLK_LOOP_WHILE; blk.loop_start=cs.here();
        if (!emit_condition_jump(cs,t,2,t.size(),true,blk)) return;
        cs.blk_stack.push_back(blk); return;
    }
    if (t[1].kind==TK::TK_INT) {
        if (t.size()<3||!is_ident(t[2],"times")) { cs.error("expected 'times' after loop count"); return; }
        if (t[1].ival<=0) { cs.error("loop count must be positive"); return; }
        uint8_t ctr; if (!cs.alloc_temp(ctr)) return;
        cs.emit(OP_SET_INT); cs.emit(ctr); cs.emit_i32(t[1].ival);
        blk.kind=BLK_LOOP_TIMES; blk.loop_start=cs.here();
        blk.counter_id=ctr; blk.owns_counter=true;
        cs.blk_stack.push_back(blk); return;
    }
    cs.error("expected: 'loop <N> times' or 'loop while <cond>'");
}

/* ─────────────────────── Functions ──────────────────────────────── */

static void parse_func(CS& cs, const Toks& t) {
    if (cs.in_func) { cs.error("nested 'func' is not allowed"); return; }
    if (t.size()<2||t[1].kind!=TK::TK_IDENT||NsaLexer::is_keyword(t[1].text)) {
        cs.error("expected function name after 'func'"); return;
    }
    const std::string& fname=t[1].text;
    if (cs.funcs.count(fname)) { cs.error("function '"+fname+"' already defined"); return; }

    FuncDef fd; fd.name=fname;
    bool arrow=false;
    for (size_t i=2;i<t.size();i++) {
        if (t[i].kind==TK::TK_OP && t[i].text=="->") {
            arrow=true;
            if (i+1>=t.size()||t[i+1].kind!=TK::TK_IDENT||NsaLexer::is_keyword(t[i+1].text)) {
                cs.error("expected return variable name after '->'"); return;
            }
            fd.has_ret=true; fd.ret_name=t[i+1].text; fd.ret_type=SYM_INT;
            i++; continue;
        }
        if (arrow) continue;
        if (t[i].kind!=TK::TK_IDENT||NsaLexer::is_keyword(t[i].text)) {
            cs.error("expected parameter name"); return;
        }
        ParamDef p; p.name=t[i].text; p.type=SYM_INT;
        fd.params.push_back(p);
    }
    if (fd.params.size()>(size_t)(NSA_MAX_LOCALS-1)) {
        cs.error("too many parameters (max 63)"); return;
    }

    cs.emit(OP_JMP_FWD);
    size_t patch_pos=cs.here(); cs.emit_u16(0);
    fd.addr=cs.here();
    cs.funcs[fname]=fd;

    cs.in_func=true; cs.cur_func_name=fname;
    cs.locals.clear(); cs.next_local=0;

    for (size_t i=0;i<fd.params.size();i++) {
        Symbol s{}; s.id=(uint8_t)i; s.type=SYM_INT; s.is_internal=false;
        cs.locals[fd.params[i].name]=s;
    }
    cs.next_local=(uint8_t)fd.params.size();
    if (fd.has_ret) {
        Symbol s{}; s.id=cs.next_local; s.type=SYM_INT; s.is_internal=false;
        cs.locals[fd.ret_name]=s;
        cs.next_local++;
    }

    Block blk{}; blk.kind=BLK_FUNC; blk.fwd_patch_pos=patch_pos;
    blk.loop_start=fd.addr;
    cs.blk_stack.push_back(blk);
}

static void parse_return(CS& cs, const Toks&) {
    if (!cs.in_func) { cs.error("'return' outside of function"); return; }
    cs.emit(OP_RET);
}

static void parse_endfunc(CS& cs, const Toks&) {
    if (cs.blk_stack.empty()||cs.blk_stack.back().kind!=BLK_FUNC) {
        cs.error("'endfunc' without matching 'func'"); return;
    }
    Block blk=cs.blk_stack.back(); cs.blk_stack.pop_back();
    cs.emit(OP_RET);
    size_t span=cs.here()-blk.fwd_patch_pos-2;
    if (span>0xFFFF) { cs.error("function body too large"); return; }
    cs.patch_u16(blk.fwd_patch_pos,(uint16_t)span);
    if (cs.funcs.count(cs.cur_func_name))
        cs.funcs[cs.cur_func_name].addr=blk.loop_start;
    cs.in_func=false; cs.cur_func_name.clear();
    cs.locals.clear(); cs.next_local=0;
}

static void parse_call(CS& cs, const Toks& t) {
    if (t.size()<2||t[1].kind!=TK::TK_IDENT) {
        cs.error("expected: call <function> [args...] [-> retvar]"); return;
    }
    const std::string& fname=t[1].text;
    if (!cs.funcs.count(fname)) { cs.error("unknown function '"+fname+"'"); return; }
    FuncDef& fd=cs.funcs[fname];

    std::vector<std::string> args;
    std::string ret_dst; bool has_ret_dst=false;
    for (size_t i=2;i<t.size();i++) {
        if (t[i].kind==TK::TK_OP&&t[i].text=="->") {
            i++;
            if (i>=t.size()||t[i].kind!=TK::TK_IDENT) {
                cs.error("expected variable name after '->'"); return;
            }
            ret_dst=t[i].text; has_ret_dst=true;
        } else if (t[i].kind==TK::TK_IDENT&&!NsaLexer::is_keyword(t[i].text)) {
            args.push_back(t[i].text);
        }
    }

    if (args.size()!=fd.params.size()) {
        char buf[128];
        snprintf(buf,sizeof(buf),"function '%s' expects %d argument(s), got %d",
                 fname.c_str(),(int)fd.params.size(),(int)args.size());
        cs.error(buf); return;
    }

    for (size_t i=0;i<args.size();i++) {
        uint8_t gid; SymType gt; if (!cs.lookup(args[i],gid,&gt)) return;
        cs.emit(OP_LOAD_ARG); cs.emit((uint8_t)i); cs.emit(gid);
    }

    cs.emit(OP_CALL);
    if (fd.addr!=0) {
        cs.emit_u16((uint16_t)fd.addr);
    } else {
        cs.call_patches.push_back({cs.here(),fname});
        cs.emit_u16(0);
    }

    if (has_ret_dst) {
        if (!fd.has_ret) { cs.error("function '"+fname+"' has no return value"); return; }
        uint8_t local_ret=(uint8_t)fd.params.size();
        uint8_t gdst; if (!cs.intern(ret_dst,fd.ret_type,gdst)) return;
        cs.emit(OP_STORE_RET); cs.emit(gdst); cs.emit(local_ret);
    }
}

/* ─────────────────────── end ────────────────────────────────────── */

static void parse_end(CS& cs, const Toks&) {
    if (cs.blk_stack.empty()) { cs.error("'end' without matching 'if' or 'loop'"); return; }
    if (cs.blk_stack.back().kind==BLK_FUNC) {
        cs.error("use 'endfunc' to close a function, not 'end'"); return;
    }
    Block blk=cs.blk_stack.back(); cs.blk_stack.pop_back();
    switch (blk.kind) {
    case BLK_IF: case BLK_ELSE: {
        size_t span=cs.here()-blk.fwd_patch_pos-2;
        if (span>0xFFFF) { cs.error("block too large"); return; }
        cs.patch_u16(blk.fwd_patch_pos,(uint16_t)span); break;
    }
    case BLK_LOOP_TIMES: {
        cs.emit(OP_DEC); cs.emit(blk.counter_id);
        size_t after=cs.here()+1+1+2;
        size_t dist=after-blk.loop_start;
        if (dist>0xFFFF) { cs.error("loop body too large"); return; }
        cs.emit(OP_JMP_BACK_NZ); cs.emit(blk.counter_id); cs.emit_u16((uint16_t)dist); break;
    }
    case BLK_LOOP_WHILE: {
        size_t after=cs.here()+1+2;
        size_t dist=after-blk.loop_start;
        if (dist>0xFFFF) { cs.error("loop body too large"); return; }
        cs.emit(OP_JMP_BACK); cs.emit_u16((uint16_t)dist);
        size_t fwd=cs.here()-blk.fwd_patch_pos-2;
        if (fwd>0xFFFF) { cs.error("loop too large"); return; }
        cs.patch_u16(blk.fwd_patch_pos,(uint16_t)fwd); break;
    }
    default: break;
    }
}

/* ─────────────────────── Line dispatcher ────────────────────────── */

static void parse_line(CS& cs, const Toks& t) {
    if (t.empty()) return;
    if (t[0].kind!=TK::TK_IDENT) { cs.error("expected statement, got '"+t[0].text+"'"); return; }
    const std::string& kw=t[0].text;
    if (kw=="let")     { parse_let(cs,t);                 return; }
    if (kw=="copy")    { parse_copy(cs,t);                return; }
    if (kw=="print")   { parse_print(cs,t,true);          return; }
    if (kw=="println") { parse_print(cs,t,false);         return; }
    if (kw=="input")   { parse_input(cs,t);               return; }
    if (kw=="inc")     { parse_inc_dec(cs,t,OP_INC);      return; }
    if (kw=="dec")     { parse_inc_dec(cs,t,OP_DEC);      return; }
    if (kw=="not")     { parse_unary(cs,t,OP_NOT);        return; }
    if (kw=="neg")     { parse_unary(cs,t,OP_NEG);        return; }
    if (kw=="concat")  { parse_concat(cs,t);              return; }
    if (kw=="len")     { parse_len(cs,t);                 return; }
    if (kw=="to_int")  { parse_to_int(cs,t);              return; }
    if (kw=="to_str")  { parse_to_str(cs,t);              return; }
    if (kw=="cmp")     { parse_cmp(cs,t);                 return; }
    if (kw=="and")     { parse_logical(cs,t,OP_AND);      return; }
    if (kw=="or")      { parse_logical(cs,t,OP_OR);       return; }
    if (kw=="if")      { parse_if(cs,t);                  return; }
    if (kw=="else")    { parse_else(cs,t);                return; }
    if (kw=="end")     { parse_end(cs,t);                 return; }
    if (kw=="loop")    { parse_loop(cs,t);                return; }
    if (kw=="func")    { parse_func(cs,t);                return; }
    if (kw=="endfunc") { parse_endfunc(cs,t);             return; }
    if (kw=="return")  { parse_return(cs,t);              return; }
    if (kw=="call")    { parse_call(cs,t);                return; }
    if (kw=="arr")     { parse_arr(cs,t);                 return; }
    if (kw=="aget")    { parse_aget(cs,t);                return; }
    if (kw=="aset")    { parse_aset(cs,t);                return; }
    if (kw=="alen")    { parse_alen(cs,t);                return; }
    for (int i=0;ARITH_TABLE[i].kw;i++)
        if (kw==ARITH_TABLE[i].kw) { parse_arith(cs,t,ARITH_TABLE[i]); return; }
    cs.error("unknown statement '"+kw+"'");
}

/* ─────────────────────── Public entry point ─────────────────────── */

CompileResult compile(const std::string& source, const char* filename) {
    CompileResult result;
    CS cs; cs.filename=filename;
    std::istringstream stream(source);
    std::string raw_line;
    while (std::getline(stream,raw_line)) {
        cs.line++;
        std::vector<NsaLexer::Token> tokens;
        std::string lex_err;
        if (!NsaLexer::tokenize(raw_line,tokens,lex_err)) {
            fprintf(stderr,"%s:%d: error: %s\n",filename,cs.line,lex_err.c_str());
            cs.errors++; continue;
        }
        if (tokens.empty()) continue;
        parse_line(cs,tokens);
    }
    for (size_t i=0;i<cs.blk_stack.size();i++) {
        BlockKind k=cs.blk_stack[i].kind;
        const char* s=(k==BLK_FUNC)?"func":(k==BLK_ELSE)?"else":
                      (k==BLK_LOOP_TIMES||k==BLK_LOOP_WHILE)?"loop":"if";
        fprintf(stderr,"%s: error: unclosed '%s' block\n",filename,s);
        cs.errors++;
    }
    for (auto& p:cs.call_patches) {
        auto it=cs.funcs.find(p.second);
        if (it==cs.funcs.end()||it->second.addr==0) {
            fprintf(stderr,"%s: error: call to undefined function '%s'\n",filename,p.second.c_str());
            cs.errors++;
        } else { cs.patch_u16(p.first,(uint16_t)it->second.addr); }
    }
    result.error_count=cs.errors;
    result.warning_count=cs.warnings;
    if (cs.errors>0) return result;
    cs.emit(OP_HALT);
    if (cs.bytecode.size()>0xFFFF) {
        fprintf(stderr,"%s: error: program too large (> 64KB)\n",filename);
        return result;
    }
    result.ok=true;
    result.bytecode=cs.bytecode;
    result.sym_count=cs.next_global;
    return result;
}

} // namespace NsaCompiler