/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 danko1122q / nusaOS project */

#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include <map>

namespace NsaCompiler {

/* ── Symbol types (shared between compiler and NSS loader) ───────────── */
enum SymType { SYM_INT, SYM_STR, SYM_BOOL, SYM_ARRAY, SYM_FILE, SYM_FLOAT };

/* ── Parameter / return info for exported functions ──────────────────── */
struct ParamDef { std::string name; SymType type; };

struct FuncDef {
    std::string            name;
    std::vector<ParamDef>  params;
    bool                   has_ret;
    std::string            ret_name;
    SymType                ret_type;
    size_t                 addr;       /* byte offset inside bytecode   */
    bool                   exported;   /* visible via 'import'          */
    FuncDef() : has_ret(false), ret_type(SYM_INT), addr(0), exported(false) {}
};

/* ── Global variable exported from a .nss module ─────────────────────── */
struct GlobalDef {
    std::string name;
    SymType     type;
    uint8_t     slot;      /* index in module's own global slot table   */
    SymType     elem_type; /* for arrays                                */
    uint8_t     arr_size;
    GlobalDef() : type(SYM_INT), slot(0), elem_type(SYM_INT), arr_size(0) {}
};

/* ── NSS module descriptor (populated by compile_nss / load_nss) ──────── */
struct NssModule {
    std::string                      name;       /* module name (no ext)     */
    std::map<std::string, FuncDef>   funcs;      /* exported functions       */
    std::map<std::string, GlobalDef> globals;    /* exported global vars     */
    std::vector<uint8_t>             init_code;  /* bytecode for init block  */
    uint8_t                          global_count; /* total slots used        */
    NssModule() : global_count(0) {}
};

/* ── Compile result ──────────────────────────────────────────────────── */
struct CompileResult {
    bool                 ok;
    int                  error_count;
    int                  warning_count;
    std::vector<uint8_t> bytecode;
    uint8_t              sym_count;
    CompileResult() : ok(false), error_count(0), warning_count(0), sym_count(0) {}
};

/* ── Public API ──────────────────────────────────────────────────────── */

/*
 * compile() — compile a .nsa source file.
 *
 * nss_load_path: directories to search for .nss modules, colon-separated.
 *   If empty, the directory of filename is used.
 *   Pass nullptr to use only the source file's directory.
 */
CompileResult compile(const std::string& source,
                      const char* filename      = "<input>",
                      const char* nss_load_path = nullptr);

/*
 * compile_nss() — parse and export a .nss module file.
 * Returns false and prints errors on stderr on failure.
 */
bool compile_nss(const std::string& source,
                 const char*  filename,
                 NssModule&   out_module);

} // namespace NsaCompiler
