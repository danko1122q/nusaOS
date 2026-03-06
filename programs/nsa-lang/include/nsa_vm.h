/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 danko1122q / nusaOS project */

#pragma once
#include <stdint.h>
#include <vector>

namespace NsaVM {

int run(const std::vector<uint8_t>& bc, int sym_count, const char* prog_name = "nsa-run");

} // namespace NsaVM