/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 danko1122q */

#pragma once
#include <stdint.h>
#include <string>
#include <vector>

namespace NusaLexer {

enum TokenKind { TK_IDENT, TK_INT, TK_STRING, TK_OP, TK_EOF };

struct Token {
    TokenKind   kind;
    std::string text;
    int32_t     ival;
};

bool tokenize(const std::string& line, std::vector<Token>& out, std::string& error_out);
bool is_keyword(const std::string& s);
bool is_valid_ident(const std::string& s);

} // namespace NusaLexer