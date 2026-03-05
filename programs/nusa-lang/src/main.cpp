/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 danko1122q */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string>

#include "nusa_compiler.h"
#include "nusa_opcodes.h"

static void print_usage(const char* prog) {
    fprintf(stderr,
        "NusaLang Compiler v1.0\n"
        "Usage: %s <input.nusa> <output.nbin>\n"
        "\n"
        "  let x = 42                  integer variable\n"
        "  let s = \"hello\"             string variable\n"
        "  print \"text\" / print x\n"
        "  add/sub/mul/div x 10        x op= 10\n"
        "  add/sub/mul/div x y         x op= y\n"
        "  if x == 10 then ... else ... end\n"
        "  loop 5 times ... end\n"
        "  loop while x != 0 ... end\n"
        "  // comment  or  # comment\n",
        prog
    );
}

static bool write_exact(int fd, const void* buf, size_t n) {
    size_t total = 0;
    while (total < n) {
        ssize_t w = write(fd, (const char*)buf + total, n - total);
        if (w <= 0) return false;
        total += w;
    }
    return true;
}

int main(int argc, char** argv) {
    if (argc < 3) { print_usage(argv[0]); return 1; }

    const char* src_path = argv[1];
    const char* out_path = argv[2];

    char resolved_src[4096], resolved_out[4096];
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        if (src_path[0] != '/') {
            snprintf(resolved_src, sizeof(resolved_src), "%s/%s", cwd, src_path);
            src_path = resolved_src;
        }
        if (out_path[0] != '/') {
            snprintf(resolved_out, sizeof(resolved_out), "%s/%s", cwd, out_path);
            out_path = resolved_out;
        }
    }

    int src_fd = open(src_path, O_RDONLY);
    if (src_fd < 0) {
        fprintf(stderr, "nusa-build: couldn't open '%s': %s\n", src_path, strerror(errno));
        return 1;
    }

    std::string source;
    char chunk[4096];
    ssize_t n;
    while ((n = read(src_fd, chunk, sizeof(chunk))) > 0)
        source.append(chunk, n);
    close(src_fd);

    NusaCompiler::CompileResult result = NusaCompiler::compile(source, src_path);
    if (!result.ok) {
        fprintf(stderr, "nusa-build: compilation failed (%d error%s)\n",
                result.error_count, result.error_count == 1 ? "" : "s");
        return 1;
    }

    int out_fd = open(out_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd < 0) {
        fprintf(stderr, "nusa-build: couldn't create '%s': %s\n", out_path, strerror(errno));
        return 1;
    }

    uint8_t  sym_count = result.sym_count;
    uint16_t bc_size   = (uint16_t)result.bytecode.size();

    bool ok = true;
    ok = ok && write_exact(out_fd, NUSA_MAGIC,             6);
    ok = ok && write_exact(out_fd, &sym_count,             1);
    ok = ok && write_exact(out_fd, &bc_size,               2);
    ok = ok && write_exact(out_fd, result.bytecode.data(), bc_size);
    close(out_fd);

    if (!ok) {
        fprintf(stderr, "nusa-build: write error on '%s'\n", out_path);
        return 1;
    }

    fprintf(stderr, "%s: OK  %u bytes, %d var(s) -> %s\n",
            src_path, (unsigned)bc_size, (int)sym_count, out_path);
    return 0;
}