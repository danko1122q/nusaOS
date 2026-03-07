/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 danko1122q / nusaOS project */

/*
 * nsa — NSA language toolchain
 *
 * Commands:
 *   nsa build <file.nsa> [output.nbin]   compile .nsa → .nbin
 *   nsa run   <file.nbin>                execute a .nbin program
 *   nsa help                             show this help
 *   nsa version                          print version info
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string>
#include <vector>

#include "nsa_compiler.h"
#include "nsa_vm.h"
#include "nsa_opcodes.h"

#define NSA_VERSION "2.2.0"
#define NSA_CODENAME "NusaOS"

/* ── Helpers ─────────────────────────────────────────────────────────── */

static bool write_exact(int fd, const void* buf, size_t n) {
    size_t total = 0;
    while (total < n) {
        ssize_t w = write(fd, (const char*)buf + total, n - total);
        if (w <= 0) return false;
        total += w;
    }
    return true;
}

static bool read_exact(int fd, void* buf, size_t n) {
    size_t total = 0;
    while (total < n) {
        ssize_t r = read(fd, (char*)buf + total, n - total);
        if (r <= 0) return false;
        total += r;
    }
    return true;
}

/* Resolve path relative to cwd when it's not absolute */
static std::string resolve_path(const char* path) {
    if (path[0] == '/') return std::string(path);
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        std::string r(cwd);
        r += '/';
        r += path;
        return r;
    }
    return std::string(path);
}

/* Replace extension of a filename */
static std::string replace_ext(const std::string& path, const std::string& new_ext) {
    size_t dot = path.rfind('.');
    size_t sep = path.rfind('/');
    if (dot != std::string::npos && (sep == std::string::npos || dot > sep))
        return path.substr(0, dot) + new_ext;
    return path + new_ext;
}

/* ── Help ────────────────────────────────────────────────────────────── */
static void print_help(const char* prog) {
    fprintf(stdout,
        "NSA Language Toolchain v" NSA_VERSION " (" NSA_CODENAME ")\n"
        "\n"
        "Usage:\n"
        "  %s build <file.nsa> [output.nbin]   Compile a .nsa source file\n"
        "  %s run   <file.nbin>                Run a compiled .nbin program\n"
        "  %s help                             Show this help message\n"
        "  %s version                          Print version information\n"
        "\n"
        "Language reference:\n"
        "\n"
        "  Variables:\n"
        "    let x = 42             declare integer variable\n"
        "    let s = \"hello\"        declare string variable\n"
        "    let b = true           declare bool variable (true/false)\n"
        "    let y = x              copy variable\n"
        "    copy dst src           copy value between variables\n"
        "\n"
        "  Output / Input:\n"
        "    print x                print variable (with newline)\n"
        "    print \"text\"           print string literal (with newline)\n"
        "    println x              print without trailing newline\n"
        "    input x                read from stdin into variable\n"
        "\n"
        "  Arithmetic:\n"
        "    add x 10               x = x + 10\n"
        "    sub x y                x = x - y\n"
        "    mul x 3                x = x * 3\n"
        "    div x y                x = x / y\n"
        "    mod x 7                x = x %% 7\n"
        "    inc x                  x = x + 1\n"
        "    dec x                  x = x - 1\n"
        "    neg x                  x = -x\n"
        "\n"
        "  Logic / Comparison:\n"
        "    not b                  b = !b  (bool or int)\n"
        "    cmp result a == b      result = (a == b)  integers: all 6 ops\n"
        "    cmp result s1 == s2    result = (s1 == s2) strings: == and != only\n"
        "    and result a b         result = a && b\n"
        "    or  result a b         result = a || b\n"
        "\n"
        "  String operations:\n"
        "    concat s \" world\"      append literal to string\n"
        "    concat s t             append string variable t to s\n"
        "    len n s                n = length of string s\n"
        "    to_int n s             n = parse s as integer\n"
        "    to_str s n             s = string representation of integer n\n"
        "\n"
        "  Arrays:\n"
        "    arr int  scores 5      declare int array of 5 elements (init 0)\n"
        "    arr str  names  3      declare string array  (init \"\")\n"
        "    arr bool flags  4      declare bool array    (init false)\n"
        "    aset scores 0 100      scores[0] = 100  (literal or variable index)\n"
        "    aget val scores i      val = scores[i]\n"
        "    alen n scores          n = declared size of array\n"
        "\n"
        "  Functions:\n"
        "    func add a b -> result define function with params and return value\n"
        "        let result = 0\n"
        "        add result a\n"
        "        add result b\n"
        "    endfunc\n"
        "    call add x y -> sum    call function, capture return value\n"
        "    return                 exit function early\n"
        "\n"
        "  Control flow:\n"
        "    if x == 10 then        compare integer to literal\n"
        "    if flag then           truthy test (int, bool, str)\n"
        "    else\n"
        "    end\n"
        "\n"
        "    loop 5 times           fixed-count loop\n"
        "    end\n"
        "\n"
        "    loop while x != 0      while loop  [also ==, <, >, <=, >=]\n"
        "    loop while flag        truthy while loop\n"
        "    end\n"
        "\n"
        "  Comments:\n"
        "    // this is a comment\n"
        "    # this is also a comment\n",
        prog, prog, prog, prog
    );
}

/* ── build command ───────────────────────────────────────────────────── */
static int cmd_build(int argc, char** argv) {
    /* argv[0] = "build", argv[1] = src, argv[2] (optional) = out */
    if (argc < 2) {
        fprintf(stderr, "nsa build: missing source file\n"
                        "usage: nsa build <file.nsa> [output.nbin]\n");
        return 1;
    }

    std::string src_path = resolve_path(argv[1]);
    std::string out_path;
    if (argc >= 3)
        out_path = resolve_path(argv[2]);
    else
        out_path = replace_ext(src_path, ".nbin");

    /* Read source */
    int src_fd = open(src_path.c_str(), O_RDONLY);
    if (src_fd < 0) {
        fprintf(stderr, "nsa build: couldn't open '%s': %s\n",
                src_path.c_str(), strerror(errno));
        return 1;
    }

    std::string source;
    char chunk[4096];
    ssize_t n;
    while ((n = read(src_fd, chunk, sizeof(chunk))) > 0)
        source.append(chunk, (size_t)n);
    close(src_fd);

    /* Compile */
    NsaCompiler::CompileResult result = NsaCompiler::compile(source, src_path.c_str());

    if (result.warning_count > 0)
        fprintf(stderr, "nsa build: %d warning(s)\n", result.warning_count);

    if (!result.ok) {
        fprintf(stderr, "nsa build: compilation failed (%d error%s)\n",
                result.error_count, result.error_count == 1 ? "" : "s");
        return 1;
    }

    /* Write .nbin */
    int out_fd = open(out_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0755);
    if (out_fd < 0) {
        fprintf(stderr, "nsa build: couldn't create '%s': %s\n",
                out_path.c_str(), strerror(errno));
        return 1;
    }

    uint8_t  sym_count = result.sym_count;
    uint16_t bc_size   = (uint16_t)result.bytecode.size();

    bool ok = true;
    ok = ok && write_exact(out_fd, NSA_MAGIC,              sizeof(NSA_MAGIC));
    ok = ok && write_exact(out_fd, &sym_count,             1);
    ok = ok && write_exact(out_fd, &bc_size,               2);
    ok = ok && write_exact(out_fd, result.bytecode.data(), bc_size);
    close(out_fd);

    if (!ok) {
        fprintf(stderr, "nsa build: write error on '%s'\n", out_path.c_str());
        return 1;
    }

    fprintf(stdout, "nsa build: OK  %u byte(s) bytecode, %d var(s)  →  %s\n",
            (unsigned)bc_size, (int)sym_count, out_path.c_str());
    return 0;
}

/* ── run command ─────────────────────────────────────────────────────── */
static int cmd_run(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "nsa run: missing program file\n"
                        "usage: nsa run <file.nbin>\n");
        return 1;
    }

    std::string path = resolve_path(argv[1]);

    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "nsa run: couldn't open '%s': %s\n",
                path.c_str(), strerror(errno));
        return 1;
    }

    /* Read & validate header: 6 magic + 1 sym_count + 2 bc_size = 9 bytes */
    char header[9];
    if (!read_exact(fd, header, 9)) {
        fprintf(stderr, "nsa run: '%s': file too small\n", path.c_str());
        close(fd); return 1;
    }
    if (memcmp(header, NSA_MAGIC, sizeof(NSA_MAGIC)) != 0) {
        fprintf(stderr, "nsa run: '%s': not a valid .nbin file "
                        "(wrong magic or version)\n", path.c_str());
        close(fd); return 1;
    }

    uint8_t  sym_count = (uint8_t)header[6];
    uint16_t bc_size   = (uint8_t)header[7] | ((uint16_t)(uint8_t)header[8] << 8);

    if (sym_count > NSA_MAX_VARS) {
        fprintf(stderr, "nsa run: '%s': corrupt header (sym_count=%u)\n",
                path.c_str(), (unsigned)sym_count);
        close(fd); return 1;
    }

    std::vector<uint8_t> bytecode(bc_size);
    if (bc_size > 0 && !read_exact(fd, bytecode.data(), bc_size)) {
        fprintf(stderr, "nsa run: '%s': truncated bytecode\n", path.c_str());
        close(fd); return 1;
    }
    close(fd);

    return NsaVM::run(bytecode, (int)sym_count, argv[1]);
}

/* ── Entry point ─────────────────────────────────────────────────────── */
int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "NSA Language Toolchain v" NSA_VERSION "\n"
                        "usage: nsa <command> [args]\n"
                        "       nsa help   for full documentation\n");
        return 1;
    }

    const char* cmd = argv[1];

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
        print_help(argv[0]);
        return 0;
    }

    if (strcmp(cmd, "version") == 0 || strcmp(cmd, "--version") == 0 || strcmp(cmd, "-v") == 0) {
        fprintf(stdout, "NSA Language Toolchain v" NSA_VERSION " (" NSA_CODENAME ")\n"
                        "Built for nusaOS — GPL-3.0-or-later\n");
        return 0;
    }

    if (strcmp(cmd, "build") == 0)
        return cmd_build(argc - 1, argv + 1);

    if (strcmp(cmd, "run") == 0)
        return cmd_run(argc - 1, argv + 1);

    fprintf(stderr, "nsa: unknown command '%s'\n"
                    "     run 'nsa help' for usage\n", cmd);
    return 1;
}