/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 danko1122q / nusaOS project */

#pragma once
#include <stdint.h>
#include <string>
#include <vector>

namespace NsaLexer {

enum TokenKind {
    TK_IDENT,       /* keyword or identifier              */
    TK_INT,         /* 42, -7                             */
    TK_STRING,      /* "hello\n"  (already unescaped)     */
    TK_BOOL,        /* true / false                       */
    TK_OP,          /* = + - * / % == != < > <= >= && ||  */
    TK_EOF
};

struct Token {
    TokenKind   kind;
    std::string text;
    int32_t     ival;   /* TK_INT: parsed value; TK_BOOL: 0/1 */
};

bool tokenize(const std::string& line, std::vector<Token>& out, std::string& error_out);
bool is_keyword(const std::string& s);
bool is_valid_ident(const std::string& s);

} // namespace NsaLexer