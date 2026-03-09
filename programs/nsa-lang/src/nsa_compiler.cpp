/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 danko1122q / nusaOS project */


#include "nsa_compiler.h"
#include "nsa_lexer.h"
#include "nsa_opcodes.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <cstring>
#include <vector>
#include <map>
#include <set>
#include <sstream>
#include <algorithm>

/* ── POSIX path helpers (nusaOS has unistd / fcntl / string.h) ─────── */
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

namespace NsaCompiler {

/* ─────────────────────────────────────────────────────────────────────
 *  Internal symbol & block types (same as before, extended)
 * ──────────────────────────────────────────────────────────────────── */

struct Symbol {
    uint8_t id;
    SymType type;
    bool    is_internal;
    bool    is_module_global; /* lives in module's global slot, not main table */
    uint8_t module_slot;      /* original slot inside the module               */
    /* Array-specific */
    SymType elem_type;
    uint8_t arr_size;
    Symbol() : id(0), type(SYM_INT), is_internal(false),
               is_module_global(false), module_slot(0),
               elem_type(SYM_INT), arr_size(0) {}
};

enum BlockKind { BLK_IF, BLK_ELSE, BLK_LOOP_TIMES, BLK_LOOP_WHILE, BLK_FUNC };
struct Block {
    BlockKind kind;
    size_t    fwd_patch_pos;
    size_t    loop_start;
    uint8_t   counter_id;
    bool      owns_counter;
};

/* ── Imported module tracking ─────────────────────────────────────── */
struct ImportedModule {
    NssModule            mod;
    /* Mapping: module global slot → main program global slot.
       Only slots that are actually referenced get mapped.              */
    std::map<uint8_t, uint8_t> slot_map;
};

/* ─────────────────────────────────────────────────────────────────────
 *  Compiler state
 * ──────────────────────────────────────────────────────────────────── */
struct CS {
    /* --- globals & locals --- */
    std::map<std::string, Symbol>   globals;
    uint8_t next_global = 0;

    std::map<std::string, Symbol>   locals;
    uint8_t next_local  = 0;
    bool    in_func     = false;
    std::string cur_func_name;

    /* --- bytecode --- */
    std::vector<uint8_t>            bytecode;
    std::vector<Block>              blk_stack;

    /* --- functions (own + imported) --- */
    std::map<std::string, FuncDef>  funcs;
    /* call-site patches: (bytecode offset, qualified function name) */
    std::vector<std::pair<size_t,std::string>> call_patches;

    /* --- imported modules --- */
    std::map<std::string, ImportedModule> imports; /* key = module name  */

    /* --- tracking which imported symbols are actually used --- */
    std::set<std::string> used_funcs;   /* "modname.funcname"            */
    std::set<std::string> used_globals; /* "modname.varname"             */

    /* --- diagnostics --- */
    int         line     = 0;
    int         errors   = 0;
    int         warnings = 0;
    const char* filename = "<input>";

    /* --- NSS search path (colon-separated directories) --- */
    std::string nss_path;

    /* ── Emit helpers ── */
    void emit(uint8_t b)         { bytecode.push_back(b); }
    void emit_u16(uint16_t v)    { bytecode.push_back(v&0xFF); bytecode.push_back((v>>8)&0xFF); }
    void emit_i32(int32_t v)     {
        bytecode.push_back(v&0xFF); bytecode.push_back((v>>8)&0xFF);
        bytecode.push_back((v>>16)&0xFF); bytecode.push_back((v>>24)&0xFF);
    }
    void emit_f64(double d) {
        uint64_t bits; memcpy(&bits,&d,8);
        for (int i=0;i<8;i++) bytecode.push_back((uint8_t)(bits>>(i*8)));
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

    /* ── Variable lookup ──
     * Order: locals (if in func) → program globals → module globals   */
    bool lookup(const std::string& name, uint8_t& id_out,
                SymType* type_out=nullptr, Symbol** sym_out=nullptr) {
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

    /* Lookup a module global — allocates a main-table slot on first use */
    bool lookup_module_global(const std::string& mod_name,
                              const std::string& var_name,
                              uint8_t& id_out, SymType* type_out=nullptr) {
        auto mit = imports.find(mod_name);
        if (mit==imports.end()) {
            error("module '"+mod_name+"' not imported"); return false;
        }
        ImportedModule& im = mit->second;
        auto git = im.mod.globals.find(var_name);
        if (git==im.mod.globals.end()) {
            error("'"+var_name+"' not found in module '"+mod_name+"'"); return false;
        }
        const GlobalDef& gd = git->second;
        /* Allocate a slot in main program if this is the first reference */
        auto sit = im.slot_map.find(gd.slot);
        if (sit==im.slot_map.end()) {
            uint8_t need = (gd.type==SYM_ARRAY) ? (uint8_t)(1+gd.arr_size) : 1;
            if ((int)next_global+need>NSA_MAX_VARS) {
                error("too many variables (importing '"+mod_name+"."+var_name+"')");
                return false;
            }
            im.slot_map[gd.slot] = next_global;
            uint8_t mapped_slot = next_global;
            next_global = (uint8_t)(next_global + need);
            /* Mirror into globals map so normal lookup works too */
            std::string alias = mod_name+"."+var_name;
            Symbol s{};
            s.id=mapped_slot; s.type=gd.type;
            s.is_module_global=true;   s.module_slot=gd.slot;
            s.elem_type=gd.elem_type;  s.arr_size=gd.arr_size;
            globals[alias]=s;
            /* Track usage */
            used_globals.insert(mod_name+"."+var_name);
            /* Emit init code for this global using the NSS init_code.
             * The NSS init_code contains OP_SET_* <old_slot> <value>.
             * We scan it for the SET opcode for gd.slot and re-emit
             * with the remapped slot id.                              */
            const std::vector<uint8_t>& ic = im.mod.init_code;
            size_t ip2 = 0;
            while (ip2 < ic.size()) {
                uint8_t iop = ic[ip2++];
                if ((NsaOpcode)iop == OP_SET_INT && ip2+5<=ic.size()) {
                    uint8_t slot_id = ic[ip2]; int32_t val;
                    val = (int32_t)((uint32_t)ic[ip2+1]|((uint32_t)ic[ip2+2]<<8)|
                                   ((uint32_t)ic[ip2+3]<<16)|((uint32_t)ic[ip2+4]<<24));
                    ip2+=5;
                    if (slot_id==gd.slot) {
                        emit(OP_SET_INT); emit(mapped_slot);
                        emit(val&0xFF); emit((val>>8)&0xFF);
                        emit((val>>16)&0xFF); emit((val>>24)&0xFF);
                    }
                } else if ((NsaOpcode)iop == OP_SET_STR && ip2+2<=ic.size()) {
                    uint8_t slot_id = ic[ip2++];
                    uint8_t slen    = ic[ip2++];
                    if (slot_id==gd.slot && ip2+slen<=ic.size()) {
                        emit(OP_SET_STR); emit(mapped_slot); emit(slen);
                        for (int si=0;si<slen;si++) emit(ic[ip2+si]);
                    }
                    ip2+=slen;
                } else if ((NsaOpcode)iop == OP_SET_BOOL && ip2+2<=ic.size()) {
                    uint8_t slot_id = ic[ip2++];
                    uint8_t bval    = ic[ip2++];
                    if (slot_id==gd.slot) {
                        emit(OP_SET_BOOL); emit(mapped_slot); emit(bval);
                    }
                } else if ((NsaOpcode)iop == OP_JMP_FWD && ip2+2<=ic.size()) {
                    /* skip over function body */
                    uint16_t off=(uint16_t)(ic[ip2]|((uint16_t)ic[ip2+1]<<8));
                    ip2+=2; ip2+=off;
                } else {
                    /* Unknown or unrelated opcode — stop scanning */
                    break;
                }
            }
        }
        id_out = im.slot_map[gd.slot];
        if (type_out) *type_out = gd.type;
        return true;
    }

    /* ── Variable intern ── */
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
            Symbol s{}; s.id=id_out; s.type=type;
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
        Symbol s{}; s.id=id_out; s.type=type;
        globals[name]=s;
        if (sym_out) *sym_out=&globals[name];
        return true;
    }

    bool alloc_slots(uint8_t count, uint8_t& first_id) {
        if (in_func) {
            if ((int)next_local+count>NSA_MAX_LOCALS) {
                error("not enough local slots for array (need "+std::to_string(count)+")");
                return false;
            }
            first_id=next_local; next_local=(uint8_t)(next_local+count); return true;
        }
        if ((int)next_global+count>NSA_MAX_VARS) {
            error("not enough variable slots for array (need "+std::to_string(count)+")");
            return false;
        }
        first_id=next_global; next_global=(uint8_t)(next_global+count); return true;
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

/* ─────────────────────────────────────────────────────────────────────
 *  NSS compiler state (simpler — only collects exports)
 * ──────────────────────────────────────────────────────────────────── */
struct NssCS {
    std::map<std::string, GlobalDef> globals;
    std::map<std::string, FuncDef>   funcs;
    uint8_t next_global = 0;
    std::vector<uint8_t> bytecode; /* init block bytecode */

    /* Minimal local tracking for function bodies (used during parsing) */
    std::map<std::string, Symbol> locals;
    uint8_t next_local = 0;
    bool    in_func    = false;
    std::string cur_func;

    std::vector<Block> blk_stack;
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
    void emit_f64(double d) {
        uint64_t bits; memcpy(&bits,&d,8);
        for (int i=0;i<8;i++) bytecode.push_back((uint8_t)(bits>>(i*8)));
    }
    void patch_u16(size_t pos, uint16_t v) {
        bytecode[pos]=(v&0xFF); bytecode[pos+1]=((v>>8)&0xFF);
    }
    size_t here() const { return bytecode.size(); }
    void error(const std::string& m) {
        fprintf(stderr,"%s:%d: error: %s\n",filename,line,m.c_str()); errors++;
    }
    void warn(const std::string& m) {
        fprintf(stderr,"%s:%d: warning: %s\n",filename,line,m.c_str()); warnings++;
    }

    bool lookup(const std::string& name, uint8_t& id_out, SymType* type_out=nullptr) {
        if (in_func) {
            auto it=locals.find(name);
            if (it!=locals.end()) {
                id_out=it->second.id;
                if (type_out) *type_out=it->second.type;
                return true;
            }
        }
        auto it=globals.find(name);
        if (it==globals.end()) { error("undeclared variable '"+name+"'"); return false; }
        id_out=it->second.slot;
        if (type_out) *type_out=it->second.type;
        return true;
    }

    bool intern_local(const std::string& name, SymType type, uint8_t& id_out) {
        auto it=locals.find(name);
        if (it!=locals.end()) { id_out=it->second.id; return true; }
        if (next_local>=NSA_MAX_LOCALS) { error("too many locals"); return false; }
        id_out=next_local++;
        Symbol s{}; s.id=id_out; s.type=type;
        locals[name]=s;
        return true;
    }

    bool alloc_temp(uint8_t& id_out) {
        if (in_func) {
            if (next_local>=NSA_MAX_LOCALS) { error("too many locals"); return false; }
            id_out=next_local++; return true;
        }
        if (next_global>=200) { error("too many module globals"); return false; }
        id_out=next_global++; return true;
    }
};

/* ─────────────────────────────────────────────────────────────────────
 *  Common helpers
 * ──────────────────────────────────────────────────────────────────── */
using Tok  = NsaLexer::Token;
using Toks = std::vector<Tok>;
using TK   = NsaLexer::TokenKind;

static bool is_ident(const Tok& t, const char* s) {
    return t.kind==TK::TK_IDENT && t.text==s;
}
static void emit_str_lit(std::vector<uint8_t>& bc, const std::string& s) {
    bc.push_back((uint8_t)s.size());
    for (char c : s) bc.push_back((uint8_t)c);
}

/* Read file from disk — used for .nss loading */
static bool read_file(const std::string& path, std::string& out) {
    int fd=open(path.c_str(),O_RDONLY);
    if (fd<0) return false;
    char chunk[4096]; ssize_t n;
    while ((n=read(fd,chunk,sizeof(chunk)))>0) out.append(chunk,(size_t)n);
    close(fd);
    return true;
}

/* Dirname of a path */
static std::string dir_of(const std::string& path) {
    size_t sep=path.rfind('/');
    if (sep==std::string::npos) return ".";
    return path.substr(0,sep);
}

/* Strip block comments — same as original */
static bool strip_block_comments(std::string& src, const char* filename) {
    size_t i=0, n=src.size();
    int start_line=1, cur_line=1;
    bool in_string=false, errors=false;
    while (i<n) {
        if (src[i]=='\n') { cur_line++; i++; continue; }
        if (!in_string && src[i]=='"') { in_string=true; i++; continue; }
        if (in_string) {
            if (src[i]=='\\' && i+1<n) { i+=2; continue; }
            if (src[i]=='"') in_string=false;
            i++; continue;
        }
        if ((src[i]=='/' && i+1<n && src[i+1]=='/')||src[i]=='#') {
            while (i<n && src[i]!='\n') i++; continue;
        }
        if (src[i]=='/' && i+1<n && src[i+1]=='*') {
            start_line=cur_line; src[i]=' '; src[i+1]=' '; i+=2;
            bool closed=false;
            while (i<n) {
                if (src[i]=='\n') { cur_line++; i++; continue; }
                if (src[i]=='*' && i+1<n && src[i+1]=='/') {
                    src[i]=' '; src[i+1]=' '; i+=2; closed=true; break;
                }
                src[i]=' '; i++;
            }
            if (!closed) {
                fprintf(stderr,"%s:%d: error: unterminated block comment\n",filename,start_line);
                errors=true;
            }
            continue;
        }
        i++;
    }
    return !errors;
}

/* ─────────────────────────────────────────────────────────────────────
 *  Forward declarations for parse functions
 * ──────────────────────────────────────────────────────────────────── */
static void parse_line(CS& cs, const Toks& t);
static void nss_parse_line(NssCS& cs, const Toks& t);

/* ═══════════════════════════════════════════════════════════════════
 *  NSA COMPILER — statement parsers (ported from original, extended)
 * ═══════════════════════════════════════════════════════════════════ */

/* ── let ──────────────────────────────────────────────────────────── */
static void parse_let(CS& cs, const Toks& t) {
    if (t.size()<4) { cs.error("expected: let <n> = <value>"); return; }
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
        cs.emit(OP_SET_STR); cs.emit(id); emit_str_lit(cs.bytecode,val.text); return;
    }
    if (val.kind==TK::TK_INT) {
        uint8_t id; if (!cs.intern(name,SYM_INT,id)) return;
        cs.emit(OP_SET_INT); cs.emit(id); cs.emit_i32(val.ival); return;
    }
    if (val.kind==TK::TK_FLOAT) {
        uint8_t id; if (!cs.intern(name,SYM_FLOAT,id)) return;
        cs.emit(OP_SET_FLOAT); cs.emit(id); cs.emit_f64(val.dval); return;
    }
    /* let x = -3.14  →  tokens: [-, TK_FLOAT(3.14)] */
    if (val.kind==TK::TK_OP && val.text=="-" && t.size()>=5 && t[4].kind==TK::TK_FLOAT) {
        uint8_t id; if (!cs.intern(name,SYM_FLOAT,id)) return;
        cs.emit(OP_SET_FLOAT); cs.emit(id); cs.emit_f64(-t[4].dval); return;
    }
    /* let x = -42  →  tokens: [-, TK_INT(42)] — already handled by signed int in lexer
       but if somehow split: */
    if (val.kind==TK::TK_OP && val.text=="-" && t.size()>=5 && t[4].kind==TK::TK_INT) {
        uint8_t id; if (!cs.intern(name,SYM_INT,id)) return;
        cs.emit(OP_SET_INT); cs.emit(id); cs.emit_i32(-t[4].ival); return;
    }
    if (val.kind==TK::TK_IDENT && !NsaLexer::is_keyword(val.text)) {
        /* Check for qualified name: module.var */
        size_t dot = val.text.find('.');
        if (dot!=std::string::npos) {
            std::string mod=val.text.substr(0,dot);
            std::string var=val.text.substr(dot+1);
            uint8_t src; SymType st;
            if (!cs.lookup_module_global(mod,var,src,&st)) return;
            uint8_t dst; if (!cs.intern(name,st,dst)) return;
            cs.emit(OP_COPY); cs.emit(dst); cs.emit(src); return;
        }
        uint8_t src; SymType st; if (!cs.lookup(val.text,src,&st)) return;
        uint8_t dst; if (!cs.intern(name,st,dst)) return;
        cs.emit(OP_COPY); cs.emit(dst); cs.emit(src); return;
    }
    cs.error("expected integer, string, bool, or variable after '='");
}

/* ── copy ─────────────────────────────────────────────────────────── */
static void parse_copy(CS& cs, const Toks& t) {
    if (t.size()<3) { cs.error("expected: copy <dst> <src>"); return; }
    uint8_t src; SymType st; if (!cs.lookup(t[2].text,src,&st)) return;
    uint8_t dst; if (!cs.intern(t[1].text,st,dst)) return;
    cs.emit(OP_COPY); cs.emit(dst); cs.emit(src);
}

/* ── print / println ──────────────────────────────────────────────── */
static void parse_print(CS& cs, const Toks& t, bool nl) {
    if (t.size()<2) { cs.error("expected argument after 'print'"); return; }
    const Tok& arg=t[1];
    NsaOpcode sop = nl ? OP_PRINT_STR    : OP_PRINT_STR_NL;
    NsaOpcode vop = nl ? OP_PRINT_VAR    : OP_PRINT_VAR_NL;
    if (arg.kind==TK::TK_STRING) {
        if (arg.text.size()>NSA_MAX_STR_LEN) { cs.error("string literal too long"); return; }
        cs.emit(sop); emit_str_lit(cs.bytecode,arg.text); return;
    }
    if (arg.kind==TK::TK_IDENT && !NsaLexer::is_keyword(arg.text)) {
        uint8_t id; SymType st; if (!cs.lookup(arg.text,id,&st)) return;
        if (st==SYM_FLOAT) {
            NsaOpcode fop = nl ? OP_FPRINT_NL : OP_FPRINT;
            cs.emit(fop); cs.emit(id); return;
        }
        cs.emit(vop); cs.emit(id); return;
    }
    if (arg.kind==TK::TK_FLOAT) {
        /* print float literal — store in temp first */
        uint8_t tmp; if (!cs.alloc_temp(tmp)) return;
        cs.emit(OP_SET_FLOAT); cs.emit(tmp); cs.emit_f64(arg.dval);
        NsaOpcode fop = nl ? OP_FPRINT_NL : OP_FPRINT;
        cs.emit(fop); cs.emit(tmp); return;
    }
    cs.error("expected string literal or variable after 'print'");
}

/* ── input ────────────────────────────────────────────────────────── */
static void parse_input(CS& cs, const Toks& t) {
    if (t.size()<2||t[1].kind!=TK::TK_IDENT||NsaLexer::is_keyword(t[1].text)) {
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
        case SYM_INT:   cs.emit(OP_INPUT_INT); cs.emit(sym->id); break;
        case SYM_STR:   cs.emit(OP_INPUT_STR); cs.emit(sym->id); break;
        case SYM_BOOL:  cs.error("'input' into a bool variable is not supported"); break;
        case SYM_ARRAY: cs.error("'input' into array is not supported"); break;
    }
}

/* ── inc / dec / unary ────────────────────────────────────────────── */
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

/* ── arithmetic ───────────────────────────────────────────────────── */
struct ArithDef { const char* kw; NsaOpcode op_imm; NsaOpcode op_var; };
static const ArithDef ARITH_TABLE[] = {
    {"add",OP_ADD_IMM,OP_ADD_VAR},{"sub",OP_SUB_IMM,OP_SUB_VAR},
    {"mul",OP_MUL_IMM,OP_MUL_VAR},{"div",OP_DIV_IMM,OP_DIV_VAR},
    {"mod",OP_MOD_IMM,OP_MOD_VAR},{nullptr,OP_NOP,OP_NOP}
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
    } else if (t[2].kind==TK::TK_IDENT&&!NsaLexer::is_keyword(t[2].text)) {
        uint8_t src; SymType st; if (!cs.lookup(t[2].text,src,&st)) return;
        if (st!=SYM_INT) { cs.error("'"+t[2].text+"' is not an integer"); return; }
        cs.emit(def.op_var); cs.emit(dst); cs.emit(src);
    } else { cs.error("expected integer literal or variable as operand"); }
}

/* ── string ops ───────────────────────────────────────────────────── */
static void parse_concat(CS& cs, const Toks& t) {
    if (t.size()<3) { cs.error("expected: concat <dst> <src|\"lit\">"); return; }
    uint8_t dst; SymType dt; if (!cs.lookup(t[1].text,dst,&dt)) return;
    if (dt!=SYM_STR) { cs.error("'"+t[1].text+"' is not a string"); return; }
    if (t[2].kind==TK::TK_STRING) {
        cs.emit(OP_CONCAT_LIT); cs.emit(dst); emit_str_lit(cs.bytecode,t[2].text);
    } else if (t[2].kind==TK::TK_IDENT&&!NsaLexer::is_keyword(t[2].text)) {
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

/* ── Process & OS primitives (v2.5.1+) — see nsa_parse_process.h ───── */
#include "nsa_parse_process.h"

/* ── Syscall interface (v2.5) ──────────────────────────────────────── */

/*
 * syscall dst num a b c
 *
 * Each of num/a/b/c can be:
 *   - an integer literal  (encoded as immediate i32)
 *   - an integer/bool variable (encoded as var id)
 *   - 0 or omitted (encoded as immediate 0)
 *
 * Encoding in bytecode:
 *   OP_SYSCALL  dst  [num_flags  num...]  [a_flags  a...]
 *               [b_flags  b...]  [c_flags  c...]
 *
 *   flags byte: 0x00 = var id follows (1 byte)
 *               0x01 = i32 immediate follows (4 bytes)
 *               0x02 = literal zero (no extra bytes)
 */
static void emit_syscall_arg(CS& cs, const Tok& tok) {
    if (tok.kind == TK::TK_INT) {
        if (tok.ival == 0) { cs.emit(0x02); return; }
        cs.emit(0x01); cs.emit_i32(tok.ival);
    } else if (tok.kind == TK::TK_IDENT) {
        uint8_t id; SymType st;
        if (!cs.lookup(tok.text, id, &st)) return;
        cs.emit(0x00); cs.emit(id);
    } else {
        cs.error("syscall: argument must be integer literal or variable");
    }
}

static void parse_syscall(CS& cs, const Toks& t) {
    /* syscall dst num [a] [b] [c] */
    if (t.size() < 3) {
        cs.error("expected: syscall <dst> <num> [a] [b] [c]"); return;
    }
    uint8_t dst; if (!cs.intern(t[1].text, SYM_INT, dst)) return;
    cs.emit(OP_SYSCALL);
    cs.emit(dst);
    /* num */
    emit_syscall_arg(cs, t[2]);
    /* a, b, c — default to zero if omitted */
    for (int i = 3; i <= 5; i++) {
        if ((size_t)i < t.size()) emit_syscall_arg(cs, t[i]);
        else cs.emit(0x02); /* zero */
    }
}

/*
 * sysbuf dst size
 * Allocates a raw byte buffer on the VM heap and stores its address in dst (int).
 */
static void parse_sysbuf(CS& cs, const Toks& t) {
    if (t.size() < 3) { cs.error("expected: sysbuf <dst_int> <size>"); return; }
    uint8_t dst; if (!cs.intern(t[1].text, SYM_INT, dst)) return;
    cs.emit(OP_SYSBUF_ALLOC);
    cs.emit(dst);
    if (t[2].kind == TK::TK_INT) {
        cs.emit(0x01); cs.emit_i32(t[2].ival);
    } else {
        uint8_t sz; if (!cs.lookup(t[2].text, sz)) return;
        cs.emit(0x00); cs.emit(sz);
    }
}

/*
 * bufwrite buf offset src_str
 * Writes string variable or literal into raw buffer at byte offset.
 */
static void parse_bufwrite(CS& cs, const Toks& t) {
    if (t.size() < 4) { cs.error("expected: bufwrite <buf_int> <offset> <src_str>"); return; }
    uint8_t buf; SymType bt; if (!cs.lookup(t[1].text, buf, &bt)) return;
    if (bt != SYM_INT) { cs.error("bufwrite: first arg must be buffer (int)"); return; }
    cs.emit(OP_SYSBUF_WRITE);
    cs.emit(buf);
    /* offset */
    if (t[2].kind == TK::TK_INT) { cs.emit(0x01); cs.emit_i32(t[2].ival); }
    else { uint8_t ov; if (!cs.lookup(t[2].text, ov)) return; cs.emit(0x00); cs.emit(ov); }
    /* src */
    if (t[3].kind == TK::TK_STRING) {
        uint8_t tmp; cs.intern("__bw_tmp__", SYM_STR, tmp);
        cs.emit(OP_SET_STR); cs.emit(tmp); emit_str_lit(cs.bytecode, t[3].text);
        cs.emit(0x00); cs.emit(tmp);
    } else {
        uint8_t src; SymType st; if (!cs.lookup(t[3].text, src, &st)) return;
        if (st != SYM_STR) { cs.error("bufwrite: source must be string"); return; }
        cs.emit(0x00); cs.emit(src);
    }
}

/*
 * bufread dst buf offset len
 * Reads len bytes from buffer at offset into string variable dst.
 */
static void parse_bufread(CS& cs, const Toks& t) {
    if (t.size() < 5) { cs.error("expected: bufread <dst_str> <buf_int> <offset> <len>"); return; }
    uint8_t dst; if (!cs.intern(t[1].text, SYM_STR, dst)) return;
    uint8_t buf; SymType bt; if (!cs.lookup(t[2].text, buf, &bt)) return;
    if (bt != SYM_INT) { cs.error("bufread: buf must be integer (address)"); return; }
    cs.emit(OP_SYSBUF_READ);
    cs.emit(dst);
    cs.emit(buf);
    /* offset */
    if (t[3].kind == TK::TK_INT) { cs.emit(0x01); cs.emit_i32(t[3].ival); }
    else { uint8_t ov; if (!cs.lookup(t[3].text, ov)) return; cs.emit(0x00); cs.emit(ov); }
    /* len */
    if (t[4].kind == TK::TK_INT) { cs.emit(0x01); cs.emit_i32(t[4].ival); }
    else { uint8_t lv; if (!cs.lookup(t[4].text, lv)) return; cs.emit(0x00); cs.emit(lv); }
}

/*
 * addrof dst src_str
 * Stores address of src_str's internal buffer in dst (int).
 * Use to pass string pointers directly as syscall arguments.
 */
static void parse_addrof(CS& cs, const Toks& t) {
    if (t.size() < 3) { cs.error("expected: addrof <dst_int> <src_str>"); return; }
    uint8_t dst; if (!cs.intern(t[1].text, SYM_INT, dst)) return;
    uint8_t src; SymType st; if (!cs.lookup(t[2].text, src, &st)) return;
    if (st != SYM_STR) { cs.error("addrof: source must be a string variable"); return; }
    cs.emit(OP_ADDR_OF);
    cs.emit(dst);
    cs.emit(src);
}

/* ── Float arithmetic (v2.5) ──────────────────────────────────────── */

/* Helper: resolve operand that may be float var or float literal */
static bool resolve_float_op(CS& cs, const Tok& tok, uint8_t& out_id) {
    if (tok.kind==TK::TK_FLOAT) {
        /* allocate a temp float slot */
        if (!cs.alloc_temp(out_id)) return false;
        cs.emit(OP_SET_FLOAT); cs.emit(out_id); cs.emit_f64(tok.dval);
        return true;
    }
    if (tok.kind==TK::TK_INT) {
        /* auto-promote int literal to float temp */
        if (!cs.alloc_temp(out_id)) return false;
        cs.emit(OP_SET_FLOAT); cs.emit(out_id); cs.emit_f64((double)tok.ival);
        return true;
    }
    SymType st;
    if (!cs.lookup(tok.text, out_id, &st)) return false;
    if (st==SYM_INT) {
        /* auto-promote int var to float temp */
        uint8_t tmp; if (!cs.alloc_temp(tmp)) return false;
        cs.emit(OP_ITOF); cs.emit(tmp); cs.emit(out_id);
        out_id=tmp;
    } else if (st!=SYM_FLOAT) {
        cs.error("float operation: expected float/int operand, got '"+tok.text+"'");
        return false;
    }
    return true;
}

/* fadd/fsub/fmul/fdiv  dst  a  b */
static void parse_fmath(CS& cs, const Toks& t, NsaOpcode op) {
    if (t.size()<4) { cs.error("expected: fadd/fsub/fmul/fdiv <dst> <a> <b>"); return; }
    uint8_t dst; if (!cs.intern(t[1].text,SYM_FLOAT,dst)) return;
    uint8_t va; if (!resolve_float_op(cs,t[2],va)) return;
    uint8_t vb; if (!resolve_float_op(cs,t[3],vb)) return;
    cs.emit(op); cs.emit(dst); cs.emit(va); cs.emit(vb);
}

/* fneg dst */
static void parse_fneg(CS& cs, const Toks& t) {
    if (t.size()<2) { cs.error("expected: fneg <var>"); return; }
    uint8_t id; SymType st; if (!cs.lookup(t[1].text,id,&st)) return;
    if (st!=SYM_FLOAT) { cs.error("fneg: not a float variable"); return; }
    cs.emit(OP_FNEG); cs.emit(id);
}

/* itof dst src_int  — int variable → float variable */
static void parse_itof(CS& cs, const Toks& t) {
    if (t.size()<3) { cs.error("expected: itof <dst_float> <src_int>"); return; }
    uint8_t dst; if (!cs.intern(t[1].text,SYM_FLOAT,dst)) return;
    uint8_t src; SymType st; if (!cs.lookup(t[2].text,src,&st)) return;
    if (st!=SYM_INT) { cs.error("itof: source must be integer"); return; }
    cs.emit(OP_ITOF); cs.emit(dst); cs.emit(src);
}

/* ftoi dst src_float  — float → int (truncate) */
static void parse_ftoi(CS& cs, const Toks& t) {
    if (t.size()<3) { cs.error("expected: ftoi <dst_int> <src_float>"); return; }
    uint8_t dst; if (!cs.intern(t[1].text,SYM_INT,dst)) return;
    uint8_t src; SymType st; if (!cs.lookup(t[2].text,src,&st)) return;
    if (st!=SYM_FLOAT) { cs.error("ftoi: source must be float"); return; }
    cs.emit(OP_FTOI); cs.emit(dst); cs.emit(src);
}

/* ftos dst src_float  — float → string */
static void parse_ftos(CS& cs, const Toks& t) {
    if (t.size()<3) { cs.error("expected: ftos <dst_str> <src_float>"); return; }
    uint8_t dst; if (!cs.intern(t[1].text,SYM_STR,dst)) return;
    uint8_t src; SymType st; if (!cs.lookup(t[2].text,src,&st)) return;
    if (st!=SYM_FLOAT) { cs.error("ftos: source must be float"); return; }
    cs.emit(OP_FTOS); cs.emit(dst); cs.emit(src);
}

/* fcmp dst  a  op  b  — float comparison → bool */
static void parse_fcmp(CS& cs, const Toks& t) {
    if (t.size()<5) { cs.error("expected: fcmp <bool_dst> <a> <op> <b>"); return; }
    uint8_t dst; if (!cs.intern(t[1].text,SYM_BOOL,dst)) return;
    uint8_t va; if (!resolve_float_op(cs,t[2],va)) return;
    if (t[3].kind!=TK::TK_OP) { cs.error("fcmp: expected comparison operator"); return; }
    const std::string& op=t[3].text;
    uint8_t vb; if (!resolve_float_op(cs,t[4],vb)) return;
    uint8_t op_byte;
    if      (op=="==") op_byte=0; else if(op=="!=") op_byte=1;
    else if (op=="<")  op_byte=2; else if(op==">")  op_byte=3;
    else if (op=="<=") op_byte=4; else if(op==">=") op_byte=5;
    else { cs.error("fcmp: unknown operator '"+op+"'"); return; }
    cs.emit(OP_FCMP); cs.emit(dst); cs.emit(va); cs.emit(op_byte); cs.emit(vb);
}

/* ── String indexing (v2.5) ───────────────────────────────────────── */

/* sget dst_str src_str idx_var  — get char at index, store as 1-char string */
static void parse_sget(CS& cs, const Toks& t) {
    if (t.size()<4) { cs.error("expected: sget <dst> <str> <idx>"); return; }
    uint8_t dst; if (!cs.intern(t[1].text,SYM_STR,dst)) return;
    uint8_t src; SymType st; if (!cs.lookup(t[2].text,src,&st)) return;
    if (st!=SYM_STR) { cs.error("sget: '"+t[2].text+"' is not a string"); return; }
    uint8_t idx; SymType it; if (!cs.lookup(t[3].text,idx,&it)) return;
    if (it!=SYM_INT) { cs.error("sget: index must be integer"); return; }
    cs.emit(OP_SGET); cs.emit(dst); cs.emit(src); cs.emit(idx);
}

/* sset dst_str idx_var char_str  — replace char at index with first char of char_str */
static void parse_sset(CS& cs, const Toks& t) {
    if (t.size()<4) { cs.error("expected: sset <str> <idx> <char_str>"); return; }
    uint8_t dst; SymType dt; if (!cs.lookup(t[1].text,dst,&dt)) return;
    if (dt!=SYM_STR) { cs.error("sset: '"+t[1].text+"' is not a string"); return; }
    uint8_t idx; SymType it; if (!cs.lookup(t[2].text,idx,&it)) return;
    if (it!=SYM_INT) { cs.error("sset: index must be integer"); return; }
    uint8_t src; SymType st; if (!cs.lookup(t[3].text,src,&st)) return;
    if (st!=SYM_STR) { cs.error("sset: char source must be string"); return; }
    cs.emit(OP_SSET); cs.emit(dst); cs.emit(idx); cs.emit(src);
}

/* ssub dst_str src_str start_var len_var  — extract substring */
static void parse_ssub(CS& cs, const Toks& t) {
    if (t.size()<5) { cs.error("expected: ssub <dst> <src> <start> <len>"); return; }
    uint8_t dst; if (!cs.intern(t[1].text,SYM_STR,dst)) return;
    uint8_t src; SymType st; if (!cs.lookup(t[2].text,src,&st)) return;
    if (st!=SYM_STR) { cs.error("ssub: '"+t[2].text+"' is not a string"); return; }
    uint8_t start; SymType stt; if (!cs.lookup(t[3].text,start,&stt)) return;
    if (stt!=SYM_INT) { cs.error("ssub: start must be integer"); return; }
    uint8_t len; SymType lt; if (!cs.lookup(t[4].text,len,&lt)) return;
    if (lt!=SYM_INT) { cs.error("ssub: len must be integer"); return; }
    cs.emit(OP_SSUB); cs.emit(dst); cs.emit(src); cs.emit(start); cs.emit(len);
}

/* ── File I/O (v2.5) ───────────────────────────────────────────────── */

/* fopen fd_var path mode_str
 *   mode_str may be a string variable or a string literal "r"/"w"/"a" */
static void parse_fopen(CS& cs, const Toks& t) {
    if (t.size()<4) { cs.error("expected: fopen <fd> <path> <mode>"); return; }
    uint8_t fd;   if (!cs.intern(t[1].text,SYM_FILE,fd)) return;
    uint8_t path; SymType pt; if (!cs.lookup(t[2].text,path,&pt)) return;
    if (pt!=SYM_STR) { cs.error("fopen: path must be a string variable"); return; }
    /* mode can be a variable or a literal */
    if (t[3].kind==TK::TK_STRING) {
        /* inline literal — store in a temp string var */
        uint8_t tmp; cs.intern("__fopen_mode__",SYM_STR,tmp);
        cs.emit(OP_SET_STR); cs.emit(tmp); emit_str_lit(cs.bytecode,t[3].text);
        cs.emit(OP_FOPEN); cs.emit(fd); cs.emit(path); cs.emit(tmp);
    } else {
        uint8_t mode; SymType mt; if (!cs.lookup(t[3].text,mode,&mt)) return;
        if (mt!=SYM_STR) { cs.error("fopen: mode must be a string"); return; }
        cs.emit(OP_FOPEN); cs.emit(fd); cs.emit(path); cs.emit(mode);
    }
}

/* fclose fd_var */
static void parse_fclose(CS& cs, const Toks& t) {
    if (t.size()<2) { cs.error("expected: fclose <fd>"); return; }
    uint8_t fd; SymType ft; if (!cs.lookup(t[1].text,fd,&ft)) return;
    if (ft!=SYM_FILE) { cs.error("fclose: '"+t[1].text+"' is not a file handle"); return; }
    cs.emit(OP_FCLOSE); cs.emit(fd);
}

/* fread dst_str fd_var */
static void parse_fread(CS& cs, const Toks& t) {
    if (t.size()<3) { cs.error("expected: fread <dst_str> <fd>"); return; }
    uint8_t dst; if (!cs.intern(t[1].text,SYM_STR,dst)) return;
    uint8_t fd;  SymType ft; if (!cs.lookup(t[2].text,fd,&ft)) return;
    if (ft!=SYM_FILE) { cs.error("fread: '"+t[2].text+"' is not a file handle"); return; }
    cs.emit(OP_FREAD); cs.emit(dst); cs.emit(fd);
}

/* fwrite fd_var src_str */
static void parse_fwrite(CS& cs, const Toks& t) {
    if (t.size()<3) { cs.error("expected: fwrite <fd> <str>"); return; }
    uint8_t fd; SymType ft; if (!cs.lookup(t[1].text,fd,&ft)) return;
    if (ft!=SYM_FILE) { cs.error("fwrite: '"+t[1].text+"' is not a file handle"); return; }
    if (t[2].kind==TK::TK_STRING) {
        uint8_t tmp; cs.intern("__fwrite_tmp__",SYM_STR,tmp);
        cs.emit(OP_SET_STR); cs.emit(tmp); emit_str_lit(cs.bytecode,t[2].text);
        cs.emit(OP_FWRITE); cs.emit(fd); cs.emit(tmp);
    } else {
        uint8_t src; SymType st; if (!cs.lookup(t[2].text,src,&st)) return;
        if (st!=SYM_STR) { cs.error("fwrite: source must be a string"); return; }
        cs.emit(OP_FWRITE); cs.emit(fd); cs.emit(src);
    }
}

/* fexists dst_bool path_var_or_literal */
static void parse_fexists(CS& cs, const Toks& t) {
    if (t.size()<3) { cs.error("expected: fexists <bool_dst> <path>"); return; }
    uint8_t dst; if (!cs.intern(t[1].text,SYM_BOOL,dst)) return;
    if (t[2].kind==TK::TK_STRING) {
        uint8_t tmp; cs.intern("__fexists_tmp__",SYM_STR,tmp);
        cs.emit(OP_SET_STR); cs.emit(tmp); emit_str_lit(cs.bytecode,t[2].text);
        cs.emit(OP_FEXISTS); cs.emit(dst); cs.emit(tmp);
    } else {
        uint8_t path; SymType pt; if (!cs.lookup(t[2].text,path,&pt)) return;
        if (pt!=SYM_STR) { cs.error("fexists: path must be a string"); return; }
        cs.emit(OP_FEXISTS); cs.emit(dst); cs.emit(path);
    }
}

/* ── cmp ──────────────────────────────────────────────────────────── */
static void parse_cmp(CS& cs, const Toks& t) {
    /* cmp <dst_bool> <a_var> <op> <b_var_or_literal>
     *
     * Right operand (b) can be:
     *   - a variable identifier
     *   - an integer literal (positive)
     *   - a negative integer literal: two tokens TK_OP("-") + TK_INT
     *
     * When b is a literal we synthesise a hidden __cmp_lit__ variable,
     * assign it the literal value, and emit the normal var-vs-var opcode.
     */
    if (t.size()<5) { cs.error("expected: cmp <bool_var> <a> <op> <b>"); return; }
    uint8_t dst; if (!cs.intern(t[1].text,SYM_BOOL,dst)) return;
    uint8_t va; SymType vta; if (!cs.lookup(t[2].text,va,&vta)) return;
    if (t[3].kind!=TK::TK_OP) { cs.error("expected comparison operator"); return; }
    const std::string& op=t[3].text;

    /* ── resolve right operand ── */
    uint8_t vb; SymType vtb;
    bool b_is_lit  = false;
    int32_t b_ival = 0;

    /* negative literal: TK_OP("-") followed by TK_INT */
    if (t[4].kind==TK::TK_OP && t[4].text=="-" && t.size()>=6 && t[5].kind==TK::TK_INT) {
        b_is_lit = true;
        b_ival   = -(int32_t)t[5].ival;
    } else if (t[4].kind==TK::TK_INT) {
        b_is_lit = true;
        b_ival   = (int32_t)t[4].ival;
    } else {
        if (!cs.lookup(t[4].text,vb,&vtb)) return;
    }

    if (b_is_lit) {
        /* intern a hidden slot for the literal */
        if (!cs.intern("__cmp_rhs__",SYM_INT,vb)) return;
        cs.emit(OP_SET_INT); cs.emit(vb); cs.emit_i32(b_ival);
        vtb = SYM_INT;
    }

    /* ── string comparison ── */
    if (vta==SYM_STR||vtb==SYM_STR) {
        if (vta!=SYM_STR||vtb!=SYM_STR) { cs.error("cmp: both operands must be string"); return; }
        if (op=="==") { cs.emit(OP_SCMP_EQ); cs.emit(dst); cs.emit(va); cs.emit(vb); return; }
        if (op=="!=") { cs.emit(OP_SCMP_NE); cs.emit(dst); cs.emit(va); cs.emit(vb); return; }
        cs.error("string comparison only supports == and !="); return;
    }

    /* ── integer/bool comparison ── */
    if (vta!=SYM_INT&&vta!=SYM_BOOL) { cs.error("cmp: left operand must be int/bool"); return; }
    if (vtb!=SYM_INT&&vtb!=SYM_BOOL) { cs.error("cmp: right operand must be int/bool"); return; }
    NsaOpcode cop;
    if      (op=="==") cop=OP_CMP_EQ; else if(op=="!=") cop=OP_CMP_NE;
    else if (op=="<")  cop=OP_CMP_LT; else if(op==">")  cop=OP_CMP_GT;
    else if (op=="<=") cop=OP_CMP_LE; else if(op==">=") cop=OP_CMP_GE;
    else { cs.error("unknown operator '"+op+"'"); return; }
    cs.emit(cop); cs.emit(dst); cs.emit(va); cs.emit(vb);
}

/* ── and / or ─────────────────────────────────────────────────────── */
static void parse_logical(CS& cs, const Toks& t, NsaOpcode op) {
    if (t.size()<4) { cs.error("expected: and/or <dst> <a> <b>"); return; }
    uint8_t dst; if (!cs.intern(t[1].text,SYM_BOOL,dst)) return;
    uint8_t va; SymType ta; if (!cs.lookup(t[2].text,va,&ta)) return;
    uint8_t vb; SymType tb; if (!cs.lookup(t[3].text,vb,&tb)) return;
    if ((ta!=SYM_INT&&ta!=SYM_BOOL)||(tb!=SYM_INT&&tb!=SYM_BOOL)) {
        cs.error("and/or requires int or bool operands"); return;
    }
    cs.emit(op); cs.emit(dst); cs.emit(va); cs.emit(vb);
}

/* ── condition jump emitter (shared by if and loop while) ─────────── */
static bool emit_condition_jump(CS& cs, const Toks& t,
                                size_t cs_, size_t ce_,
                                bool invert, Block& blk) {
    if (ce_-cs_==1) {
        if (t[cs_].kind!=TK::TK_IDENT||NsaLexer::is_keyword(t[cs_].text)) {
            cs.error("expected variable in condition"); return false;
        }
        uint8_t id; if (!cs.lookup(t[cs_].text,id)) return false;
        cs.emit(invert?OP_JMP_IF_TRUE:OP_JMP_IF_FALSE);
        cs.emit(id); blk.fwd_patch_pos=cs.here(); cs.emit_u16(0); return true;
    }
    if (ce_-cs_==3) {
        uint8_t id; SymType type;
        if (!cs.lookup(t[cs_].text,id,&type)) return false;
        if (type!=SYM_INT && type!=SYM_BOOL) { cs.error("comparison condition requires integer or bool variable"); return false; }
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

/* ── if / else ────────────────────────────────────────────────────── */
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

/* ── loop ─────────────────────────────────────────────────────────── */
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

/* ── end ──────────────────────────────────────────────────────────── */
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

/* ── func / endfunc / return ──────────────────────────────────────── */
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
        if (t[i].kind==TK::TK_OP&&t[i].text=="->") {
            arrow=true;
            if (i+1>=t.size()||t[i+1].kind!=TK::TK_IDENT||NsaLexer::is_keyword(t[i+1].text)) {
                cs.error("expected return variable name after '->'"); return;
            }
            fd.has_ret=true; fd.ret_name=t[i+1].text; fd.ret_type=SYM_INT; i++; continue;
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
        Symbol s{}; s.id=(uint8_t)i; s.type=SYM_INT;
        cs.locals[fd.params[i].name]=s;
    }
    cs.next_local=(uint8_t)fd.params.size();
    if (fd.has_ret) {
        Symbol s{}; s.id=cs.next_local; s.type=SYM_INT;
        cs.locals[fd.ret_name]=s; cs.next_local++;
    }
    Block blk{}; blk.kind=BLK_FUNC; blk.fwd_patch_pos=patch_pos; blk.loop_start=fd.addr;
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

/* ── call ─────────────────────────────────────────────────────────── */
static void parse_call(CS& cs, const Toks& t) {
    if (t.size()<2||t[1].kind!=TK::TK_IDENT) {
        cs.error("expected: call <[module.]function> [args...] [-> retvar]"); return;
    }

    /* Support qualified call: call math.max a b -> result */
    std::string raw_name = t[1].text;
    std::string fname;
    std::string mod_name;
    size_t dot = raw_name.find('.');
    if (dot!=std::string::npos) {
        mod_name = raw_name.substr(0, dot);
        fname    = raw_name.substr(dot+1);
    } else {
        fname = raw_name;
    }

    /* Build the lookup key: module-qualified or plain */
    std::string fkey = mod_name.empty() ? fname : (mod_name+"."+fname);

    if (!cs.funcs.count(fkey)) {
        cs.error("unknown function '"+fkey+"'"); return;
    }
    FuncDef& fd=cs.funcs[fkey];
    if (!mod_name.empty()) cs.used_funcs.insert(fkey);

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
                 fkey.c_str(),(int)fd.params.size(),(int)args.size());
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
        cs.call_patches.push_back({cs.here(),fkey});
        cs.emit_u16(0);
    }

    if (has_ret_dst) {
        if (!fd.has_ret) { cs.error("function '"+fkey+"' has no return value"); return; }
        uint8_t local_ret=(uint8_t)fd.params.size();
        uint8_t gdst; if (!cs.intern(ret_dst,fd.ret_type,gdst)) return;
        cs.emit(OP_STORE_RET); cs.emit(gdst); cs.emit(local_ret);
    }
}

/* ── arr / aget / aset / alen ─────────────────────────────────────── */
static SymType parse_elem_type(const std::string& s) {
    if (s=="int")  return SYM_INT;
    if (s=="str")  return SYM_STR;
    if (s=="bool") return SYM_BOOL;
    return SYM_INT;
}
static void parse_arr(CS& cs, const Toks& t) {
    /* arr <type> <name> <size> */
    if (t.size()<4) { cs.error("expected: arr <type> <name> <size>"); return; }
    if (t[1].kind!=TK::TK_IDENT) { cs.error("expected element type"); return; }
    const std::string& etype_s=t[1].text;
    if (etype_s!="int"&&etype_s!="str"&&etype_s!="bool") {
        cs.error("array element type must be int, str, or bool"); return;
    }
    if (t[2].kind!=TK::TK_IDENT||NsaLexer::is_keyword(t[2].text)) {
        cs.error("expected array name"); return;
    }
    if (t[3].kind!=TK::TK_INT||t[3].ival<=0||(uint32_t)t[3].ival>NSA_MAX_ARRAY_SIZE) {
        cs.error("array size must be integer 1.."+std::to_string(NSA_MAX_ARRAY_SIZE)); return;
    }
    uint8_t sz=(uint8_t)t[3].ival;
    const std::string& aname=t[2].text;
    uint8_t base_id;
    if (!cs.alloc_slots((uint8_t)(1+sz),base_id)) return;
    Symbol arr_sym{}; arr_sym.id=base_id; arr_sym.type=SYM_ARRAY;
    arr_sym.elem_type=parse_elem_type(etype_s); arr_sym.arr_size=sz;
    if (cs.in_func) cs.locals[aname]=arr_sym; else cs.globals[aname]=arr_sym;
    /* Emit descriptor init */
    cs.emit(OP_SET_INT); cs.emit(base_id); cs.emit_i32((int32_t)sz);
    /* Default-init elements */
    for (int i=0;i<sz;i++) {
        uint8_t elem=(uint8_t)(base_id+1+i);
        if (etype_s=="int")  { cs.emit(OP_SET_INT);  cs.emit(elem); cs.emit_i32(0); }
        else if (etype_s=="str") {
            cs.emit(OP_SET_STR); cs.emit(elem);
            cs.bytecode.push_back(0); /* empty string */
        } else { cs.emit(OP_SET_BOOL); cs.emit(elem); cs.emit(0); }
    }
}
static void parse_aget(CS& cs, const Toks& t) {
    if (t.size()<4) { cs.error("expected: aget <dst> <array> <idx_var>"); return; }
    uint8_t dst; SymType dt;
    if (cs.in_func) {
        auto it=cs.locals.find(t[1].text);
        if (it!=cs.locals.end()) { dst=it->second.id; dt=it->second.type; goto got_dst; }
    }
    {
        auto it=cs.globals.find(t[1].text);
        if (it!=cs.globals.end()) { dst=it->second.id; dt=it->second.type; goto got_dst; }
        /* Allocate temp int */
        if (!cs.intern(t[1].text,SYM_INT,dst)) return;
        dt=SYM_INT;
    }
    got_dst:;
    uint8_t base; SymType bt; if (!cs.lookup(t[2].text,base,&bt)) return;
    if (bt!=SYM_ARRAY) { cs.error("'"+t[2].text+"' is not an array"); return; }
    uint8_t idx; SymType it2; if (!cs.lookup(t[3].text,idx,&it2)) return;
    if (it2!=SYM_INT) { cs.error("array index must be an integer"); return; }
    cs.emit(OP_ARR_GET); cs.emit(dst); cs.emit(base); cs.emit(idx);
    (void)dt;
}
static void parse_aset(CS& cs, const Toks& t) {
    /* aset <array> <idx_var|lit> <src_var|lit> */
    if (t.size()<4) { cs.error("expected: aset <array> <idx> <value>"); return; }
    uint8_t base; SymType bt; if (!cs.lookup(t[1].text,base,&bt)) return;
    if (bt!=SYM_ARRAY) { cs.error("'"+t[1].text+"' is not an array"); return; }
    Symbol* arr_sym=nullptr;
    if (cs.in_func) { auto it=cs.locals.find(t[1].text); if(it!=cs.locals.end()) arr_sym=&it->second; }
    if (!arr_sym)   { auto it=cs.globals.find(t[1].text); if(it!=cs.globals.end()) arr_sym=&it->second; }

    /* idx: variable only */
    if (t[2].kind!=TK::TK_IDENT||NsaLexer::is_keyword(t[2].text)) {
        cs.error("aset: index must be a variable"); return;
    }
    uint8_t idx; SymType it3; if (!cs.lookup(t[2].text,idx,&it3)) return;
    if (it3!=SYM_INT) { cs.error("aset: index must be an integer variable"); return; }

    /* value: variable or literal */
    if (t[3].kind==TK::TK_IDENT&&!NsaLexer::is_keyword(t[3].text)) {
        uint8_t src; SymType st; if (!cs.lookup(t[3].text,src,&st)) return;
        cs.emit(OP_ARR_SET); cs.emit(base); cs.emit(idx); cs.emit(src);
    } else if (t[3].kind==TK::TK_INT) {
        cs.emit(OP_ARR_SET_IMM); cs.emit(base); cs.emit(idx);
        cs.emit(0x01); cs.emit_i32(t[3].ival);
    } else if (t[3].kind==TK::TK_STRING) {
        cs.emit(OP_ARR_SET_IMM); cs.emit(base); cs.emit(idx);
        cs.emit(0x02); emit_str_lit(cs.bytecode,t[3].text);
    } else if (t[3].kind==TK::TK_BOOL) {
        cs.emit(OP_ARR_SET_IMM); cs.emit(base); cs.emit(idx);
        cs.emit(0x03); cs.emit((uint8_t)t[3].ival);
    } else { cs.error("aset: value must be a variable or literal"); }
}
static void parse_alen(CS& cs, const Toks& t) {
    if (t.size()<3) { cs.error("expected: alen <int_var> <array>"); return; }
    uint8_t dst; if (!cs.intern(t[1].text,SYM_INT,dst)) return;
    uint8_t base; SymType bt; if (!cs.lookup(t[2].text,base,&bt)) return;
    if (bt!=SYM_ARRAY) { cs.error("'"+t[2].text+"' is not an array"); return; }
    cs.emit(OP_ARR_LEN); cs.emit(dst); cs.emit(base);
}

/* ═══════════════════════════════════════════════════════════════════
 *  import  — load a .nss module and register its exported symbols
 *            Smart import: only used symbols end up in final bytecode
 * ═══════════════════════════════════════════════════════════════════ */
static void parse_import(CS& cs, const Toks& t) {
    /* import "module_name"   or   import "path/to/module" */
    if (t.size()<2||t[1].kind!=TK::TK_STRING) {
        cs.error("expected: import \"module_name\""); return;
    }
    const std::string& mod_path = t[1].text;
    if (mod_path.empty()) { cs.error("empty module name"); return; }

    /* Derive module name from last component */
    size_t slash=mod_path.rfind('/');
    std::string mod_name = (slash==std::string::npos) ? mod_path : mod_path.substr(slash+1);

    if (cs.imports.count(mod_name)) {
        cs.warn("module '"+mod_name+"' already imported"); return;
    }

    /* Find .nss file */
    std::string found_path;
    /* Search order: 1) nss_path dirs  2) dir of current source file */
    auto try_path = [&](const std::string& dir) -> bool {
        std::string p = dir + "/" + mod_path + ".nss";
        if (access(p.c_str(),R_OK)==0) { found_path=p; return true; }
        return false;
    };

    bool located = false;
    if (!cs.nss_path.empty()) {
        /* colon-separated dirs */
        std::string path_copy = cs.nss_path;
        size_t pos=0;
        while (pos<path_copy.size()) {
            size_t colon=path_copy.find(':',pos);
            std::string dir=(colon==std::string::npos)?path_copy.substr(pos):path_copy.substr(pos,colon-pos);
            if (try_path(dir)) { located=true; break; }
            if (colon==std::string::npos) break;
            pos=colon+1;
        }
    }
    if (!located) {
        /* Use dirname of current source */
        std::string src_dir = dir_of(std::string(cs.filename));
        located = try_path(src_dir);
    }
    if (!located) {
        cs.error("cannot find module '"+mod_path+"' (looked for '"+mod_path+".nss')");
        return;
    }

    std::string src;
    if (!read_file(found_path,src)) {
        cs.error("cannot read module file '"+found_path+"'");
        return;
    }

    NssModule mod;
    if (!compile_nss(src,found_path.c_str(),mod)) {
        cs.error("errors in module '"+mod_name+"'");
        return;
    }
    mod.name = mod_name;

    ImportedModule im;
    im.mod = mod;

    /* Register all exported functions into cs.funcs with qualified keys.
     * Their addresses are NOT yet resolved; call_patches handles that.  */
    for (auto& kv : mod.funcs) {
        std::string qname = mod_name + "." + kv.first;
        if (cs.funcs.count(qname)) {
            cs.warn("function '"+qname+"' already defined — skipping");
            continue;
        }
        FuncDef fd = kv.second;
        fd.addr = 0; /* unresolved — will be patched after tree-shaking   */
        fd.exported = true;
        cs.funcs[qname] = fd;
    }

    cs.imports[mod_name] = im;
}

/* ═══════════════════════════════════════════════════════════════════
 *  Tree-shaking linker
 *  After the main program is compiled, walk used_funcs and include
 *  only the reachable function bytecode from each module.
 * ═══════════════════════════════════════════════════════════════════ */

/* Returns offset where func was appended in cs.bytecode.
 *
 * Intra-module OP_CALL fix:
 * When a NSS function calls a sibling function, the OP_CALL address
 * stored in mod.init_code is relative to that init_code buffer.
 * After we copy the bytecode into the main program (at a different
 * offset), those addresses become stale.  We record every OP_CALL
 * found in the copied body as a pending call_patch so link_imports
 * can fix them up once all sibling functions have been appended.
 */
static size_t append_func_bytecode(CS& cs,
                                   const NssModule& mod,
                                   const FuncDef& fd,
                                   const std::map<uint8_t,uint8_t>& /*slot_map*/) {
    const std::vector<uint8_t>& src = mod.init_code;
    if (fd.addr >= src.size()) return cs.here();

    /* Wrap function body with a forward jump so execution skips over it */
    cs.emit(OP_JMP_FWD);
    size_t patch = cs.here(); cs.emit_u16(0);
    size_t func_start = cs.here();

    /* Copy bytes from fd.addr until OP_RET.
     * When we encounter OP_CALL, record a pending patch so we can fix
     * the callee address once all sibling functions are placed. */
    size_t ip = fd.addr;
    while (ip < src.size()) {
        uint8_t b = src[ip++];
        cs.emit(b);
        if ((NsaOpcode)b == OP_RET) break;

        if ((NsaOpcode)b == OP_CALL && ip + 1 < src.size()) {
            /* Read the old (init_code-relative) address */
            uint16_t old_addr = (uint16_t)src[ip] | ((uint16_t)src[ip+1] << 8);
            ip += 2;

            /* Find which module function lives at that address */
            std::string callee_qname;
            for (auto& fkv : mod.funcs) {
                if ((uint16_t)fkv.second.addr == old_addr) {
                    /* Build the qualified name — we need the module name.
                     * Search cs.imports to find which module this is. */
                    for (auto& imp_kv : cs.imports) {
                        if (&imp_kv.second.mod == &mod) {
                            callee_qname = imp_kv.first + "." + fkv.first;
                            break;
                        }
                    }
                    break;
                }
            }

            if (!callee_qname.empty()) {
                /* Emit placeholder and register patch site */
                cs.call_patches.push_back({cs.here(), callee_qname});
                cs.emit_u16(0);
            } else {
                /* Unknown target — copy raw bytes and hope for the best */
                cs.emit((uint8_t)(old_addr & 0xFF));
                cs.emit((uint8_t)(old_addr >> 8));
            }
        }
    }

    size_t func_end = cs.here();
    cs.patch_u16(patch,(uint16_t)(func_end-patch-2));
    return func_start;
}

/* ── Scan NSS function bytecode for intra-module OP_CALL references ─
 *
 * When funcA inside a module calls funcB (also in the same module),
 * the main program never references funcB directly — so it would be
 * silently dropped by the tree-shaker unless we scan for it here.
 *
 * We walk the raw bytecode of every already-reachable function and
 * collect any OP_CALL targets that resolve to sibling functions in
 * the same module.  Those siblings are added to used_funcs and the
 * process repeats until the reachable set stops growing (fixed-point).
 * ──────────────────────────────────────────────────────────────────── */
static void collect_transitive_calls(CS& cs) {
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto& imp_kv : cs.imports) {
            const std::string& mod_name = imp_kv.first;
            ImportedModule&    im       = imp_kv.second;
            const std::vector<uint8_t>& bc = im.mod.init_code;

            for (auto& fkv : im.mod.funcs) {
                std::string qname = mod_name + "." + fkv.first;
                if (!cs.used_funcs.count(qname)) continue; /* not yet reachable */

                const FuncDef& fd = fkv.second;
                if (fd.addr >= bc.size()) continue;

                /* Scan this function's bytecode for OP_CALL instructions */
                size_t ip = fd.addr;
                while (ip < bc.size()) {
                    uint8_t op = bc[ip++];
                    if ((NsaOpcode)op == OP_RET) break;

                    if ((NsaOpcode)op == OP_CALL && ip+1 < bc.size()) {
                        /* OP_CALL is followed by a 2-byte address.
                         * Match that address against sibling func addrs. */
                        uint16_t target = (uint16_t)bc[ip] | ((uint16_t)bc[ip+1]<<8);
                        ip += 2;
                        for (auto& sfkv : im.mod.funcs) {
                            if ((uint16_t)sfkv.second.addr == target) {
                                std::string sq = mod_name + "." + sfkv.first;
                                if (!cs.used_funcs.count(sq)) {
                                    cs.used_funcs.insert(sq);
                                    changed = true;
                                }
                            }
                        }
                        continue; /* already consumed 2 bytes */
                    }

                    /* Skip operand bytes for all other opcodes so we don't
                     * misinterpret data as an opcode on the next iteration. */
                    switch ((NsaOpcode)op) {
                        /* 0-operand */
                        case OP_NOP: case OP_HALT: case OP_RET: break;
                        /* 1-operand (var id) */
                        case OP_PRINT_VAR: case OP_PRINT_VAR_NL:
                        case OP_INPUT_INT: case OP_INPUT_STR:
                        case OP_INC: case OP_DEC: case OP_NOT: case OP_NEG:
                        case OP_ARR_LEN:
                            ip += 1; break;
                        /* 2-operand */
                        case OP_COPY: case OP_CONCAT: case OP_LEN:
                        case OP_STR_TO_INT: case OP_INT_TO_STR:
                        case OP_SCMP_EQ: case OP_SCMP_NE:
                        case OP_LOAD_ARG: case OP_STORE_RET:
                        case OP_GCOPY: case OP_GLOAD: case OP_GSTORE:
                        case OP_ARR_GET: case OP_ARR_SET:
                            ip += 2; break;
                        /* 3-operand (cmp, logic, arith-var) */
                        case OP_CMP_EQ: case OP_CMP_NE: case OP_CMP_LT:
                        case OP_CMP_GT: case OP_CMP_LE: case OP_CMP_GE:
                        case OP_AND: case OP_OR:
                        case OP_ADD_VAR: case OP_SUB_VAR: case OP_MUL_VAR:
                        case OP_DIV_VAR: case OP_MOD_VAR:
                            ip += 3; break;
                        /* var + i32 */
                        case OP_ADD_IMM: case OP_SUB_IMM: case OP_MUL_IMM:
                        case OP_DIV_IMM: case OP_MOD_IMM:
                        case OP_SET_INT: case OP_GSET_INT:
                            ip += 5; break;
                        /* var + u16 (jump) */
                        case OP_JMP_FWD: case OP_JMP_BACK:
                            ip += 2; break;
                        case OP_JMP_IF_TRUE: case OP_JMP_IF_FALSE:
                        case OP_JMP_BACK_TRUE: case OP_JMP_BACK_FALSE:
                        case OP_JMP_BACK_NZ: case OP_JMP_BACK_Z:
                            ip += 3; break;
                        case OP_JMP_IF_EQ: case OP_JMP_IF_NE: case OP_JMP_IF_LT:
                        case OP_JMP_IF_GT: case OP_JMP_IF_LE: case OP_JMP_IF_GE:
                            ip += 3; break;
                        /* SET_STR / GSET_STR / CONCAT_LIT / PRINT_STR:
                         * var + length-prefixed string — skip conservatively */
                        case OP_SET_STR: case OP_GSET_STR:
                        case OP_CONCAT_LIT: case OP_PRINT_STR: case OP_PRINT_STR_NL:
                            ip += 1; /* skip var id */
                            if (ip < bc.size()) {
                                uint8_t slen = bc[ip++];
                                ip += slen;
                            }
                            break;
                        case OP_SET_BOOL: case OP_GSET_BOOL:
                            ip += 2; break;
                        default:
                            ip += 1; break; /* unknown — skip one byte safely */
                    }
                }
            }
        }
    }
}

/* ── Link imported modules (tree-shake then append) ──────────────── */
static void link_imports(CS& cs) {
    if (cs.imports.empty()) return;

    /* Step 1: expand used_funcs transitively — catch intra-module calls
     * (funcA calls funcB inside same module; main only calls funcA directly) */
    collect_transitive_calls(cs);

    /* Step 2: append bytecode for every reachable function */
    for (auto& imp_kv : cs.imports) {
        const std::string& mod_name = imp_kv.first;
        ImportedModule&    im       = imp_kv.second;

        for (auto& fkv : im.mod.funcs) {
            std::string qname = mod_name + "." + fkv.first;
            if (!cs.used_funcs.count(qname)) continue; /* unreachable → skip */

            const FuncDef& mfd = fkv.second;

            /* Ensure all global slots referenced by this func are mapped */
            for (auto& gkv : im.mod.globals) {
                const GlobalDef& gd = gkv.second;
                if (im.slot_map.find(gd.slot)==im.slot_map.end()) {
                    uint8_t need=(gd.type==SYM_ARRAY)?(uint8_t)(1+gd.arr_size):1;
                    if ((int)cs.next_global+need<=NSA_MAX_VARS) {
                        im.slot_map[gd.slot]=cs.next_global;
                        cs.next_global=(uint8_t)(cs.next_global+need);
                    }
                }
            }

            /* Append bytecode and record the new address */
            size_t new_addr = append_func_bytecode(cs, im.mod, mfd, im.slot_map);

            /* Patch call sites */
            for (auto& p : cs.call_patches) {
                if (p.second==qname)
                    cs.patch_u16(p.first,(uint16_t)new_addr);
            }
            if (cs.funcs.count(qname))
                cs.funcs[qname].addr = new_addr;
        }
    }
}

/* ─────────────────────────────────────────────────────────────────────
 *  NSA line dispatcher
 * ──────────────────────────────────────────────────────────────────── */
static void parse_line(CS& cs, const Toks& t) {
    if (t.empty()) return;
    if (t[0].kind!=TK::TK_IDENT) { cs.error("expected statement, got '"+t[0].text+"'"); return; }
    const std::string& kw=t[0].text;
    if (kw=="import")  { parse_import(cs,t);              return; }
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
    /* string indexing (v2.5) */
    if (kw=="sget")    { parse_sget(cs,t);               return; }
    if (kw=="sset")    { parse_sset(cs,t);               return; }
    if (kw=="ssub")    { parse_ssub(cs,t);               return; }
    /* file I/O (v2.5) */
    if (kw=="fopen")   { parse_fopen(cs,t);              return; }
    if (kw=="fclose")  { parse_fclose(cs,t);             return; }
    if (kw=="fread")   { parse_fread(cs,t);              return; }
    if (kw=="fwrite")  { parse_fwrite(cs,t);             return; }
    if (kw=="fexists") { parse_fexists(cs,t);            return; }
    /* syscall interface (v2.5) */
    if (kw=="syscall")  { parse_syscall(cs,t);           return; }
    if (kw=="sysbuf")   { parse_sysbuf(cs,t);            return; }
    if (kw=="bufwrite") { parse_bufwrite(cs,t);           return; }
    if (kw=="bufread")  { parse_bufread(cs,t);            return; }
    if (kw=="addrof")   { parse_addrof(cs,t);             return; }
    /* process control (v2.5.1) */
    if (kw=="fork")     { parse_fork(cs,t);               return; }
    if (kw=="exec")     { parse_exec(cs,t);               return; }
    if (kw=="waitpid")  { parse_waitpid(cs,t);            return; }
    if (kw=="exit")     { parse_exit(cs,t);               return; }
    /* process utilities (v2.5.2) */
    if (kw=="getpid")   { parse_getpid(cs,t);             return; }
    if (kw=="sleep")    { parse_sleep(cs,t);              return; }
    if (kw=="getenv")   { parse_getenv(cs,t);             return; }
    if (kw=="peek")     { parse_peek(cs,t,OP_PEEK);       return; }
    if (kw=="poke")     { parse_poke(cs,t,OP_POKE);       return; }
    if (kw=="peek8")    { parse_peek(cs,t,OP_PEEK8);      return; }
    if (kw=="poke8")    { parse_poke(cs,t,OP_POKE8);      return; }
    /* float arithmetic (v2.5) */
    if (kw=="fadd")    { parse_fmath(cs,t,OP_FADD);      return; }
    if (kw=="fsub")    { parse_fmath(cs,t,OP_FSUB);      return; }
    if (kw=="fmul")    { parse_fmath(cs,t,OP_FMUL);      return; }
    if (kw=="fdiv")    { parse_fmath(cs,t,OP_FDIV);      return; }
    if (kw=="fneg")    { parse_fneg(cs,t);               return; }
    if (kw=="itof")    { parse_itof(cs,t);               return; }
    if (kw=="ftoi")    { parse_ftoi(cs,t);               return; }
    if (kw=="ftos")    { parse_ftos(cs,t);               return; }
    if (kw=="fcmp")    { parse_fcmp(cs,t);               return; }
    if (kw=="aget")    { parse_aget(cs,t);                return; }
    if (kw=="aset")    { parse_aset(cs,t);                return; }
    if (kw=="alen")    { parse_alen(cs,t);                return; }
    for (int i=0;ARITH_TABLE[i].kw;i++)
        if (kw==ARITH_TABLE[i].kw) { parse_arith(cs,t,ARITH_TABLE[i]); return; }
    cs.error("unknown statement '"+kw+"'");
}

/* ═══════════════════════════════════════════════════════════════════
 *  NSS COMPILER — parses a .nss module
 *
 *  .nss syntax additions vs .nsa:
 *    global int   PI = 314
 *    global str   GREETING = "Hello"
 *    global bool  DEBUG = false
 *    func  / endfunc  (same syntax, auto-exported)
 *
 *  There is NO top-level code in .nss — only global declarations
 *  and function definitions.  Attempting to write statements outside
 *  of a function or global declaration is an error.
 * ═══════════════════════════════════════════════════════════════════ */

/* NSS func parsing — reuses the same opcodes, but slot ids are module-local */
static void nss_parse_func(NssCS& cs, const Toks& t) {
    if (cs.in_func) { cs.error("nested 'func' is not allowed in .nss"); return; }
    if (t.size()<2||t[1].kind!=TK::TK_IDENT||NsaLexer::is_keyword(t[1].text)) {
        cs.error("expected function name after 'func'"); return;
    }
    const std::string& fname=t[1].text;
    if (cs.funcs.count(fname)) { cs.error("function '"+fname+"' already defined"); return; }

    FuncDef fd; fd.name=fname; fd.exported=true;
    bool arrow=false;
    for (size_t i=2;i<t.size();i++) {
        if (t[i].kind==TK::TK_OP&&t[i].text=="->") {
            arrow=true;
            if (i+1>=t.size()||t[i+1].kind!=TK::TK_IDENT) {
                cs.error("expected return variable name after '->'"); return;
            }
            fd.has_ret=true; fd.ret_name=t[i+1].text; fd.ret_type=SYM_INT; i++; continue;
        }
        if (arrow) continue;
        if (t[i].kind!=TK::TK_IDENT||NsaLexer::is_keyword(t[i].text)) {
            cs.error("expected parameter name"); return;
        }
        ParamDef p; p.name=t[i].text; p.type=SYM_INT;
        fd.params.push_back(p);
    }

    /* Emit JMP_FWD to skip body */
    cs.emit(OP_JMP_FWD);
    size_t patch=cs.here(); cs.emit_u16(0);
    fd.addr=cs.here();
    cs.funcs[fname]=fd;

    cs.in_func=true; cs.cur_func=fname;
    cs.locals.clear(); cs.next_local=0;
    for (size_t i=0;i<fd.params.size();i++) {
        Symbol s{}; s.id=(uint8_t)i; s.type=SYM_INT;
        cs.locals[fd.params[i].name]=s;
    }
    cs.next_local=(uint8_t)fd.params.size();
    if (fd.has_ret) {
        Symbol s{}; s.id=cs.next_local; s.type=SYM_INT;
        cs.locals[fd.ret_name]=s; cs.next_local++;
    }
    Block blk{}; blk.kind=BLK_FUNC; blk.fwd_patch_pos=patch; blk.loop_start=fd.addr;
    cs.blk_stack.push_back(blk);
}
static void nss_parse_endfunc(NssCS& cs) {
    if (cs.blk_stack.empty()||cs.blk_stack.back().kind!=BLK_FUNC) {
        cs.error("'endfunc' without matching 'func'"); return;
    }
    Block blk=cs.blk_stack.back(); cs.blk_stack.pop_back();
    cs.emit(OP_RET);
    size_t span=cs.here()-blk.fwd_patch_pos-2;
    if (span>0xFFFF) { cs.error("function body too large"); return; }
    cs.patch_u16(blk.fwd_patch_pos,(uint16_t)span);
    if (cs.funcs.count(cs.cur_func))
        cs.funcs[cs.cur_func].addr=blk.loop_start;
    cs.in_func=false; cs.cur_func.clear();
    cs.locals.clear(); cs.next_local=0;
}

/* Helper for NSS statements inside function bodies */
static void nss_emit_stmt(NssCS& cs, const Toks& t);

static void nss_parse_line(NssCS& cs, const Toks& t) {
    if (t.empty()) return;
    if (t[0].kind!=TK::TK_IDENT) { cs.error("expected statement"); return; }
    const std::string& kw=t[0].text;

    /* ── global declaration (top-level only) ── */
    if (kw=="global") {
        if (cs.in_func) { cs.error("'global' not allowed inside function"); return; }
        /* global <type> <name> = <value> */
        if (t.size()<5) { cs.error("expected: global <type> <name> = <value>"); return; }
        if (t[1].kind!=TK::TK_IDENT) { cs.error("expected type after 'global'"); return; }
        const std::string& type_s=t[1].text;
        if (t[2].kind!=TK::TK_IDENT||NsaLexer::is_keyword(t[2].text)) {
            cs.error("expected variable name"); return;
        }
        const std::string& gname=t[2].text;
        if (t[3].kind!=TK::TK_OP||t[3].text!="=") { cs.error("expected '='"); return; }
        const Tok& val=t[4];

        if (cs.globals.count(gname)) { cs.error("'"+gname+"' already declared"); return; }

        GlobalDef gd; gd.name=gname;
        uint8_t slot;
        if (cs.next_global>=NSA_MAX_VARS) { cs.error("too many module globals"); return; }
        slot=cs.next_global++;
        gd.slot=slot;

        if (type_s=="int") {
            if (val.kind!=TK::TK_INT) { cs.error("expected integer value"); return; }
            gd.type=SYM_INT;
            cs.emit(OP_SET_INT); cs.emit(slot); cs.emit_i32(val.ival);
        } else if (type_s=="str") {
            if (val.kind!=TK::TK_STRING) { cs.error("expected string value"); return; }
            if (val.text.size()>NSA_MAX_STR_LEN) { cs.error("string too long"); return; }
            gd.type=SYM_STR;
            cs.emit(OP_SET_STR); cs.emit(slot); emit_str_lit(cs.bytecode,val.text);
        } else if (type_s=="bool") {
            if (val.kind!=TK::TK_BOOL) { cs.error("expected bool value"); return; }
            gd.type=SYM_BOOL;
            cs.emit(OP_SET_BOOL); cs.emit(slot); cs.emit((uint8_t)val.ival);
        } else {
            cs.error("unknown type '"+type_s+"' — expected int, str, or bool"); return;
        }
        cs.globals[gname]=gd;
        return;
    }

    /* ── func / endfunc / return at top level ── */
    if (kw=="func")    { nss_parse_func(cs,t);    return; }
    if (kw=="endfunc") { nss_parse_endfunc(cs);   return; }
    if (kw=="return") {
        if (!cs.in_func) { cs.error("'return' outside function"); return; }
        cs.emit(OP_RET); return;
    }

    if (!cs.in_func) {
        cs.error("statement '"+kw+"' is not allowed at module top-level — use 'global' or 'func'");
        return;
    }

    /* ── statements inside function bodies ── */
    nss_emit_stmt(cs,t);
}

/* Emit a statement inside a NSS function body.
 * This is a reduced copy of the NSA dispatcher — same opcodes but
 * slots are local to the NssCS. */
static void nss_emit_stmt(NssCS& cs, const Toks& t) {
    const std::string& kw=t[0].text;

    auto lookup=[&](const std::string& name, uint8_t& id, SymType* tp=nullptr) -> bool {
        return cs.lookup(name,id,tp);
    };
    auto intern_local=[&](const std::string& name, SymType type, uint8_t& id) -> bool {
        return cs.intern_local(name,type,id);
    };

    /* ── let ── */
    if (kw=="let") {
        if (t.size()<4||t[2].kind!=TK::TK_OP||t[2].text!="=") {
            cs.error("expected: let <n> = <value>"); return;
        }
        const std::string& name=t[1].text;
        const Tok& val=t[3];
        uint8_t id;
        if (val.kind==TK::TK_INT) {
            intern_local(name,SYM_INT,id);
            cs.emit(OP_SET_INT); cs.emit(id); cs.emit_i32(val.ival);
        } else if (val.kind==TK::TK_STRING) {
            intern_local(name,SYM_STR,id);
            cs.emit(OP_SET_STR); cs.emit(id); emit_str_lit(cs.bytecode,val.text);
        } else if (val.kind==TK::TK_BOOL) {
            intern_local(name,SYM_BOOL,id);
            cs.emit(OP_SET_BOOL); cs.emit(id); cs.emit((uint8_t)val.ival);
        } else if (val.kind==TK::TK_IDENT&&!NsaLexer::is_keyword(val.text)) {
            uint8_t src; SymType st; if (!lookup(val.text,src,&st)) return;
            intern_local(name,st,id);
            cs.emit(OP_COPY); cs.emit(id); cs.emit(src);
        } else { cs.error("expected value after '='"); }
        return;
    }
    /* ── copy ── */
    if (kw=="copy") {
        if (t.size()<3) { cs.error("expected: copy <dst> <src>"); return; }
        uint8_t src; SymType st; if (!lookup(t[2].text,src,&st)) return;
        uint8_t dst; intern_local(t[1].text,st,dst);
        cs.emit(OP_COPY); cs.emit(dst); cs.emit(src); return;
    }
    /* ── print / println ── */
    if (kw=="print"||kw=="println") {
        bool nl=(kw=="print");
        if (t.size()<2) { cs.error("expected argument"); return; }
        if (t[1].kind==TK::TK_STRING) {
            cs.emit(nl?OP_PRINT_STR:OP_PRINT_STR_NL);
            emit_str_lit(cs.bytecode,t[1].text); return;
        }
        uint8_t id; if (!lookup(t[1].text,id)) return;
        cs.emit(nl?OP_PRINT_VAR:OP_PRINT_VAR_NL); cs.emit(id); return;
    }
    /* ── inc / dec / neg / not ── */
    if (kw=="inc"||kw=="dec"||kw=="neg"||kw=="not") {
        if (t.size()<2) { cs.error("expected variable"); return; }
        uint8_t id; SymType tp; if (!lookup(t[1].text,id,&tp)) return;
        NsaOpcode uop=(kw=="inc")?OP_INC:(kw=="dec")?OP_DEC:(kw=="neg")?OP_NEG:OP_NOT;
        cs.emit(uop); cs.emit(id); return;
    }
    /* ── arithmetic ── */
    if (kw=="add"||kw=="sub"||kw=="mul"||kw=="div"||kw=="mod") {
        if (t.size()<3) { cs.error("expected operands"); return; }
        uint8_t dst; SymType dt; if (!lookup(t[1].text,dst,&dt)) return;
        NsaOpcode op_imm=OP_NOP, op_var=OP_NOP;
        if      (kw=="add") { op_imm=OP_ADD_IMM; op_var=OP_ADD_VAR; }
        else if (kw=="sub") { op_imm=OP_SUB_IMM; op_var=OP_SUB_VAR; }
        else if (kw=="mul") { op_imm=OP_MUL_IMM; op_var=OP_MUL_VAR; }
        else if (kw=="div") { op_imm=OP_DIV_IMM; op_var=OP_DIV_VAR; }
        else                { op_imm=OP_MOD_IMM; op_var=OP_MOD_VAR; }
        if (t[2].kind==TK::TK_INT) {
            cs.emit(op_imm); cs.emit(dst); cs.emit_i32(t[2].ival);
        } else {
            uint8_t src; if (!lookup(t[2].text,src)) return;
            cs.emit(op_var); cs.emit(dst); cs.emit(src);
        }
        return;
    }
    /* ── cmp  dst  a  op  b_or_literal ── */
    if (kw=="cmp") {
        if (t.size()<5) { cs.error("expected: cmp <dst> <a> <op> <b>"); return; }
        uint8_t dst; intern_local(t[1].text,SYM_BOOL,dst);
        uint8_t va; SymType vta; if (!lookup(t[2].text,va,&vta)) return;
        if (t[3].kind!=TK::TK_OP) { cs.error("expected comparison operator"); return; }
        const std::string& op=t[3].text;
        NsaOpcode cop=OP_CMP_EQ;
        if      (op=="==") cop=OP_CMP_EQ; else if(op=="!=") cop=OP_CMP_NE;
        else if (op=="<")  cop=OP_CMP_LT; else if(op==">")  cop=OP_CMP_GT;
        else if (op=="<=") cop=OP_CMP_LE; else if(op==">=") cop=OP_CMP_GE;
        else { cs.error("unknown operator '"+op+"'"); return; }
        if (t[4].kind==TK::TK_INT) {
            /* second operand is a literal — store in temp local then compare */
            uint8_t tmp; cs.alloc_temp(tmp);
            cs.emit(OP_SET_INT); cs.emit(tmp); cs.emit_i32(t[4].ival);
            cs.emit(cop); cs.emit(dst); cs.emit(va); cs.emit(tmp);
        } else {
            uint8_t vb; if (!lookup(t[4].text,vb)) return;
            cs.emit(cop); cs.emit(dst); cs.emit(va); cs.emit(vb);
        }
        return;
    }
    /* ── and / or ── */
    if (kw=="and"||kw=="or") {
        if (t.size()<4) { cs.error("expected: and/or <dst> <a> <b>"); return; }
        uint8_t dst; intern_local(t[1].text,SYM_BOOL,dst);
        uint8_t va; if (!lookup(t[2].text,va)) return;
        uint8_t vb; if (!lookup(t[3].text,vb)) return;
        cs.emit(kw=="and"?OP_AND:OP_OR); cs.emit(dst); cs.emit(va); cs.emit(vb); return;
    }
    /* ── if / else / end ── */
    if (kw=="if") {
        size_t tp=0;
        for (size_t i=1;i<t.size();i++) if(is_ident(t[i],"then")){tp=i;break;}
        if (!tp) { cs.error("expected 'then' in if"); return; }
        /* truthy check: if var then */
        if (tp==2) {
            uint8_t id; if (!cs.lookup(t[1].text,id)) return;
            cs.emit(OP_JMP_IF_FALSE); cs.emit(id);
            Block blk{}; blk.kind=BLK_IF; blk.fwd_patch_pos=cs.here();
            cs.emit_u16(0); cs.blk_stack.push_back(blk); return;
        }
        cs.error("NSS 'if': use 'if <bool_var> then' — run cmp first"); return;
    }
    if (kw=="else") {
        if (cs.blk_stack.empty()||cs.blk_stack.back().kind!=BLK_IF) {
            cs.error("'else' without matching 'if'"); return;
        }
        Block& blk=cs.blk_stack.back();
        cs.emit(OP_JMP_FWD); size_t ep=cs.here(); cs.emit_u16(0);
        size_t span=cs.here()-blk.fwd_patch_pos-2;
        cs.patch_u16(blk.fwd_patch_pos,(uint16_t)span);
        blk.kind=BLK_ELSE; blk.fwd_patch_pos=ep; return;
    }
    if (kw=="end") {
        if (cs.blk_stack.empty()||cs.blk_stack.back().kind==BLK_FUNC) {
            cs.error("'end' without matching block"); return;
        }
        Block blk=cs.blk_stack.back(); cs.blk_stack.pop_back();
        size_t span=cs.here()-blk.fwd_patch_pos-2;
        cs.patch_u16(blk.fwd_patch_pos,(uint16_t)span); return;
    }
    /* ── loop ── */
    if (kw=="loop") {
        if (t.size()<2) { cs.error("expected: loop <N> times / loop while <var>"); return; }
        Block blk{}; blk.fwd_patch_pos=0;
        if (is_ident(t[1],"while")) {
            if (t.size()<3) { cs.error("expected condition after 'loop while'"); return; }
            blk.kind=BLK_LOOP_WHILE; blk.loop_start=cs.here();
            uint8_t id; if (!cs.lookup(t[2].text,id)) return;
            cs.emit(OP_JMP_IF_FALSE); cs.emit(id);
            blk.fwd_patch_pos=cs.here(); cs.emit_u16(0);
            cs.blk_stack.push_back(blk); return;
        }
        if (t[1].kind==TK::TK_INT) {
            if (t.size()<3||!is_ident(t[2],"times")) { cs.error("expected 'times'"); return; }
            if (t[1].ival<=0) { cs.error("loop count must be positive"); return; }
            uint8_t ctr; cs.alloc_temp(ctr);
            cs.emit(OP_SET_INT); cs.emit(ctr); cs.emit_i32(t[1].ival);
            blk.kind=BLK_LOOP_TIMES; blk.loop_start=cs.here();
            blk.counter_id=ctr; blk.owns_counter=true;
            cs.blk_stack.push_back(blk); return;
        }
        cs.error("expected: loop <N> times / loop while <var>"); return;
    }
    /* ── string ops ── */
    if (kw=="concat") {
        if (t.size()<3) { cs.error("expected: concat <dst> <src>"); return; }
        uint8_t dst; if (!lookup(t[1].text,dst)) return;
        if (t[2].kind==TK::TK_STRING) {
            cs.emit(OP_CONCAT_LIT); cs.emit(dst); emit_str_lit(cs.bytecode,t[2].text);
        } else {
            uint8_t src; if (!lookup(t[2].text,src)) return;
            cs.emit(OP_CONCAT); cs.emit(dst); cs.emit(src);
        }
        return;
    }
    if (kw=="len") {
        if (t.size()<3) { cs.error("expected: len <int> <str>"); return; }
        uint8_t dst; intern_local(t[1].text,SYM_INT,dst);
        uint8_t src; if (!lookup(t[2].text,src)) return;
        cs.emit(OP_LEN); cs.emit(dst); cs.emit(src); return;
    }
    if (kw=="to_str") {
        if (t.size()<3) { cs.error("expected: to_str <str> <int>"); return; }
        uint8_t dst; intern_local(t[1].text,SYM_STR,dst);
        uint8_t src; if (!lookup(t[2].text,src)) return;
        cs.emit(OP_INT_TO_STR); cs.emit(dst); cs.emit(src); return;
    }
    if (kw=="to_int") {
        if (t.size()<3) { cs.error("expected: to_int <int> <str>"); return; }
        uint8_t dst; intern_local(t[1].text,SYM_INT,dst);
        uint8_t src; if (!lookup(t[2].text,src)) return;
        cs.emit(OP_STR_TO_INT); cs.emit(dst); cs.emit(src); return;
    }
    /* ── return ── */
    if (kw=="return") { cs.emit(OP_RET); return; }
    /* ── call (intra-module) ── */
    if (kw=="call") {
        if (t.size()<2) { cs.error("expected function name"); return; }
        const std::string& fname=t[1].text;
        auto fit=cs.funcs.find(fname);
        if (fit==cs.funcs.end()) { cs.error("unknown function '"+fname+"'"); return; }
        FuncDef& fd=fit->second;
        std::vector<std::string> args; std::string ret_dst; bool has_ret=false;
        for (size_t i=2;i<t.size();i++) {
            if (t[i].kind==TK::TK_OP&&t[i].text=="->") {
                i++; if (i<t.size()) { ret_dst=t[i].text; has_ret=true; }
            } else if (t[i].kind==TK::TK_IDENT&&!NsaLexer::is_keyword(t[i].text)) {
                args.push_back(t[i].text);
            }
        }
        for (size_t i=0;i<args.size();i++) {
            uint8_t gid; if (!lookup(args[i],gid)) return;
            cs.emit(OP_LOAD_ARG); cs.emit((uint8_t)i); cs.emit(gid);
        }
        cs.emit(OP_CALL);
        if (fd.addr!=0) { cs.emit_u16((uint16_t)fd.addr); }
        else { cs.call_patches.push_back({cs.here(),fname}); cs.emit_u16(0); }
        if (has_ret && fd.has_ret) {
            uint8_t local_ret=(uint8_t)fd.params.size();
            uint8_t gdst; intern_local(ret_dst,fd.ret_type,gdst);
            cs.emit(OP_STORE_RET); cs.emit(gdst); cs.emit(local_ret);
        }
        return;
    }
    cs.error("unknown statement '"+kw+"' in NSS function");
}

/* ═══════════════════════════════════════════════════════════════════
 *  Public: compile_nss()
 * ═══════════════════════════════════════════════════════════════════ */
bool compile_nss(const std::string& source, const char* filename, NssModule& out) {
    std::string src=source;
    if (!strip_block_comments(src,filename)) return false;

    NssCS cs; cs.filename=filename;
    std::istringstream stream(src);
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
        nss_parse_line(cs,tokens);
    }

    /* Patch intra-module call sites */
    for (auto& p:cs.call_patches) {
        auto it=cs.funcs.find(p.second);
        if (it==cs.funcs.end()||it->second.addr==0) {
            fprintf(stderr,"%s: error: call to undefined function '%s'\n",filename,p.second.c_str());
            cs.errors++;
        } else { cs.patch_u16(p.first,(uint16_t)it->second.addr); }
    }
    /* Check unclosed blocks */
    for (size_t i=0;i<cs.blk_stack.size();i++) {
        BlockKind k=cs.blk_stack[i].kind;
        const char* s=(k==BLK_FUNC)?"func":"block";
        fprintf(stderr,"%s: error: unclosed '%s'\n",filename,s);
        cs.errors++;
    }

    if (cs.errors>0) return false;

    out.funcs       = cs.funcs;
    out.globals     = cs.globals;
    out.init_code   = cs.bytecode;
    out.global_count= cs.next_global;
    return true;
}

/* ═══════════════════════════════════════════════════════════════════
 *  Public: compile()  — main NSA compiler entry point
 * ═══════════════════════════════════════════════════════════════════ */
CompileResult compile(const std::string& source,
                      const char* filename,
                      const char* nss_load_path) {
    CompileResult result;

    std::string src=source;
    if (!strip_block_comments(src,filename)) {
        result.error_count=1; return result;
    }

    CS cs; cs.filename=filename;
    if (nss_load_path) cs.nss_path=nss_load_path;

    std::istringstream stream(src);
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

    /* Check unclosed blocks */
    for (size_t i=0;i<cs.blk_stack.size();i++) {
        BlockKind k=cs.blk_stack[i].kind;
        const char* s=(k==BLK_FUNC)?"func":(k==BLK_ELSE)?"else":
                      (k==BLK_LOOP_TIMES||k==BLK_LOOP_WHILE)?"loop":"if";
        fprintf(stderr,"%s: error: unclosed '%s' block\n",filename,s);
        cs.errors++;
    }

    /* ── Tree-shaking linker: append only used imported functions ── */
    link_imports(cs);

    /* ── Patch remaining unresolved call sites ── */
    for (auto& p:cs.call_patches) {
        auto it=cs.funcs.find(p.second);
        if (it==cs.funcs.end()||it->second.addr==0) {
            fprintf(stderr,"%s: error: call to undefined function '%s'\n",filename,p.second.c_str());
            cs.errors++;
        } else { cs.patch_u16(p.first,(uint16_t)it->second.addr); }
    }

    result.error_count   = cs.errors;
    result.warning_count = cs.warnings;
    if (cs.errors>0) return result;

    cs.emit(OP_HALT);

    if (cs.bytecode.size()>0xFFFF) {
        fprintf(stderr,"%s: error: program too large (> 64KB)\n",filename);
        return result;
    }

    result.ok        = true;
    result.bytecode  = cs.bytecode;
    result.sym_count = cs.next_global;
    return result;
}

} // namespace NsaCompiler