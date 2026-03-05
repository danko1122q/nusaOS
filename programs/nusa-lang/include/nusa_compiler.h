/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 danko1122q */

#pragma once
#include <stdint.h>
#include <string>
#include <vector>

namespace NusaCompiler {

struct CompileResult {
    bool                 ok;
    int                  error_count;
    std::vector<uint8_t> bytecode;
    uint8_t              sym_count;
    CompileResult() : ok(false), error_count(0), sym_count(0) {}
};

CompileResult compile(const std::string& source, const char* filename = "<input>");

} // namespace NusaCompiler