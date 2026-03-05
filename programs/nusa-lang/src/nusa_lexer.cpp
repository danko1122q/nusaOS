/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 danko1122q */

#include "nusa_lexer.h"
#include <stdint.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <set>

namespace NusaLexer {

static const std::set<std::string> KEYWORDS = {
    "let", "print", "add", "sub", "mul", "div",
    "if", "else", "then", "end",
    "loop", "while", "times"
};

bool is_keyword(const std::string& s) {
    return KEYWORDS.count(s) > 0;
}

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
            case 'n':  out += '\n'; break;
            case 't':  out += '\t'; break;
            case 'r':  out += '\r'; break;
            case '\\': out += '\\'; break;
            case '"':  out += '"';  break;
            default:
                err = std::string("unknown escape sequence: \\") + raw[i];
                return false;
        }
    }
    return true;
}

bool tokenize(const std::string& line, std::vector<Token>& out, std::string& error_out) {
    out.clear();
    error_out.clear();

    size_t i = 0;
    const size_t n = line.size();

    while (true) {
        while (i < n && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r'))
            i++;
        if (i >= n) break;

        if (line[i] == '#') break;
        if (i + 1 < n && line[i] == '/' && line[i+1] == '/') break;

        if (line[i] == '"') {
            i++;
            std::string raw;
            bool closed = false;
            while (i < n) {
                if (line[i] == '\\' && i + 1 < n) {
                    raw += line[i]; raw += line[i+1]; i += 2;
                } else if (line[i] == '"') {
                    closed = true; i++; break;
                } else {
                    raw += line[i++];
                }
            }
            if (!closed) { error_out = "unterminated string literal"; return false; }
            std::string unescaped;
            if (!unescape(raw, unescaped, error_out)) return false;
            Token t; t.kind = TK_STRING; t.text = unescaped; t.ival = 0;
            out.push_back(t);
            continue;
        }

        if (i + 1 < n) {
            std::string two = line.substr(i, 2);
            if (two == "==" || two == "!=") {
                Token t; t.kind = TK_OP; t.text = two; t.ival = 0;
                out.push_back(t);
                i += 2; continue;
            }
        }

        if (line[i] == '=' || line[i] == '+' || line[i] == '-' ||
            line[i] == '*' || line[i] == '/') {
            Token t; t.kind = TK_OP; t.text = std::string(1, line[i]); t.ival = 0;
            out.push_back(t);
            i++; continue;
        }

        if (isdigit((unsigned char)line[i]) ||
            ((line[i] == '-' || line[i] == '+') && i+1 < n && isdigit((unsigned char)line[i+1])))
        {
            size_t start = i;
            if (line[i] == '-' || line[i] == '+') i++;
            while (i < n && isdigit((unsigned char)line[i])) i++;
            std::string num = line.substr(start, i - start);
            errno = 0;
            char* end;
            long v = strtol(num.c_str(), &end, 10);
            if (errno != 0 || *end != '\0') {
                error_out = "invalid integer: " + num; return false;
            }
            if (v > (long)2147483647L || v < (long)(-2147483647L - 1)) {
                error_out = "integer out of 32-bit range: " + num; return false;
            }
            Token t; t.kind = TK_INT; t.text = num; t.ival = (int32_t)v;
            out.push_back(t);
            continue;
        }

        if (isalpha((unsigned char)line[i]) || line[i] == '_') {
            size_t start = i;
            while (i < n && (isalnum((unsigned char)line[i]) || line[i] == '_')) i++;
            Token t; t.kind = TK_IDENT; t.text = line.substr(start, i - start); t.ival = 0;
            out.push_back(t);
            continue;
        }

        error_out = std::string("unexpected character: '") + line[i] + "'";
        return false;
    }
    return true;
}

} // namespace NusaLexer