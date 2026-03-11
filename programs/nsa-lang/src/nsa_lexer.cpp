/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 danko1122q / nusaOS project */

#include "nsa_lexer.h"
#include <stdint.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <set>

namespace NsaLexer {

static const std::set<std::string> KEYWORDS = {
    "let", "print", "println",
    "add", "sub", "mul", "div", "mod",
    "inc", "dec", "not", "neg",
    "copy",
    "concat", "len", "to_int", "to_str",
    "input",
    "if", "else", "then", "end",
    "loop", "while", "times",
    "and", "or", "cmp",
    "true", "false",
    /* functions */
    "func", "endfunc", "return", "call",
    /* arrays */
    "arr", "aget", "aset", "alen",
    /* string indexing (v2.5) */
    "sget", "sset", "ssub",
    /* file I/O (v2.5) */
    "fopen", "fclose", "fread", "fwrite", "fexists",
    /* float (v2.5) */
    "itof", "ftoi", "ftos",
    /* syscall (v2.5) */
    "syscall", "sysbuf", "bufwrite", "bufread", "addrof",
    /* process (v2.5.1) */
    "fork", "exec", "waitpid", "exit",
    /* process utilities (v2.5.2) */
    "getpid", "sleep", "getenv",
    /* low-level memory (v2.5.2) */
    "peek", "poke", "peek8", "poke8",
    /* string utilities (v2.5.3) */
    "strcmp", "strfind", "strtrim", "strupper", "strlower",
    "strreplace", "strsplit",
    /* loop control (v2.5.3) */
    "break", "continue",
    /* quality of life (v2.5.4) */
    "printf", "swap", "abs", "min", "max",
    /* command-line & file (v2.6) */
    "argc", "argv", "freadline",
};

bool is_keyword(const std::string& s) { return KEYWORDS.count(s) > 0; }

bool is_valid_ident(const std::string& s) {
    if (s.empty()) return false;
    if (!isalpha((unsigned char)s[0]) && s[0] != '_') return false;
    for (size_t i = 0; i < s.size(); i++)
        if (!isalnum((unsigned char)s[i]) && s[i] != '_') return false;
    return !is_keyword(s);
}

static bool unescape(const std::string& raw, std::string& out, std::string& err) {
    out.clear();
    for (size_t i = 0; i < raw.size(); i++) {
        if (raw[i] != '\\') { out += raw[i]; continue; }
        i++;
        if (i >= raw.size()) { err = "trailing backslash in string"; return false; }
        switch (raw[i]) {
            case 'n':  out += '\n'; break; case 't':  out += '\t'; break;
            case 'r':  out += '\r'; break; case '0':  out += '\0'; break;
            case '\\': out += '\\'; break; case '"':  out += '"';  break;
            case '\'': out += '\''; break;
            default: err = std::string("unknown escape: \\") + raw[i]; return false;
        }
    }
    return true;
}

bool tokenize(const std::string& line, std::vector<Token>& out, std::string& error_out) {
    out.clear(); error_out.clear();
    size_t i = 0, n = line.size();

    while (true) {
        while (i < n && (line[i]==' '||line[i]=='\t'||line[i]=='\r')) i++;
        if (i >= n) break;
        if (line[i] == '#') break;
        if (i+1 < n && line[i]=='/' && line[i+1]=='/') break;
        if ((unsigned char)line[i] > 127) { i++; continue; }

        /* String literals */
        if (line[i] == '"') {
            i++;
            std::string raw; bool closed = false;
            while (i < n) {
                if (line[i]=='\\' && i+1<n) { raw+=line[i]; raw+=line[i+1]; i+=2; }
                else if (line[i]=='"') { closed=true; i++; break; }
                else { raw+=line[i++]; }
            }
            if (!closed) { error_out="unterminated string literal"; return false; }
            std::string unescaped;
            if (!unescape(raw, unescaped, error_out)) return false;
            Token t; t.kind=TK_STRING; t.text=unescaped; t.ival=0; out.push_back(t);
            continue;
        }

        /* Two-char operators */
        if (i+1 < n) {
            std::string two = line.substr(i,2);
            if (two=="=="||two=="!="||two=="<="||two==">="||two=="&&"||two=="||"||two=="->") {
                Token t; t.kind=TK_OP; t.text=two; t.ival=0; out.push_back(t);
                i+=2; continue;
            }
        }

        /* Single-char operators */
        if (line[i]=='='||line[i]=='+'||line[i]=='-'||line[i]=='*'||
            line[i]=='/'||line[i]=='%'||line[i]=='<'||line[i]=='>') {
            Token t; t.kind=TK_OP; t.text=std::string(1,line[i]); t.ival=0;
            out.push_back(t); i++; continue;
        }

        /* Numeric literals — integer or float */
        if (isdigit((unsigned char)line[i]) ||
            ((line[i]=='-'||line[i]=='+') && i+1<n && isdigit((unsigned char)line[i+1]) &&
             (out.empty()||out.back().kind==TK_OP)))
        {
            size_t start=i;
            if (line[i]=='-'||line[i]=='+') i++;
            while (i<n && isdigit((unsigned char)line[i])) i++;
            /* float: optional fractional part */
            bool is_float = (i<n && line[i]=='.' && i+1<n && isdigit((unsigned char)line[i+1]));
            if (is_float) {
                i++; /* consume dot */
                while (i<n && isdigit((unsigned char)line[i])) i++;
                /* optional exponent */
                if (i<n && (line[i]=='e'||line[i]=='E')) {
                    i++;
                    if (i<n && (line[i]=='+'||line[i]=='-')) i++;
                    while (i<n && isdigit((unsigned char)line[i])) i++;
                }
                std::string num=line.substr(start,i-start);
                errno=0; char* endp; double dv=strtod(num.c_str(),&endp);
                if (errno||*endp) { error_out="invalid float: "+num; return false; }
                Token t; t.kind=TK_FLOAT; t.text=num; t.ival=0; t.dval=dv; out.push_back(t);
            } else {
                std::string num=line.substr(start,i-start);
                errno=0; char* end; long v=strtol(num.c_str(),&end,10);
                if (errno||*end) { error_out="invalid integer: "+num; return false; }
                if (v>2147483647L||v<(-2147483647L-1)) { error_out="integer out of range: "+num; return false; }
                Token t; t.kind=TK_INT; t.text=num; t.ival=(int32_t)v; out.push_back(t);
            }
            continue;
        }

        /* Identifiers / keywords — allow one embedded dot for module.symbol */
        if (isalpha((unsigned char)line[i])||line[i]=='_') {
            size_t start=i;
            while (i<n && (isalnum((unsigned char)line[i])||line[i]=='_')) i++;
            /* Allow module.symbol — consume dot + tail if present */
            if (i<n && line[i]=='.' && i+1<n &&
                (isalpha((unsigned char)line[i+1])||line[i+1]=='_')) {
                i++; /* consume dot */
                while (i<n && (isalnum((unsigned char)line[i])||line[i]=='_')) i++;
            }
            std::string word=line.substr(start,i-start);
            if (word=="true")  { Token t; t.kind=TK_BOOL; t.text="true";  t.ival=1; out.push_back(t); }
            else if (word=="false") { Token t; t.kind=TK_BOOL; t.text="false"; t.ival=0; out.push_back(t); }
            else { Token t; t.kind=TK_IDENT; t.text=word; t.ival=0; out.push_back(t); }
            continue;
        }

        /* Parentheses and commas — silently skip (syntactic sugar only).
         * This allows  fork(pid)  or  exec(ret, path, a0)  style writing
         * without affecting the token stream that the compiler sees.    */
        if (line[i]=='('||line[i]==')'||line[i]==',') { i++; continue; }

        error_out = std::string("unexpected character: '") + line[i] + "'";
        return false;
    }
    return true;
}

} // namespace NsaLexer