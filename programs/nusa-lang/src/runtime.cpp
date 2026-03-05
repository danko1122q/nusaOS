/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 danko1122q */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <vector>

#include "nusa_vm.h"
#include "nusa_opcodes.h"

static bool read_exact(int fd, void* buf, size_t n) {
    size_t total = 0;
    while (total < n) {
        ssize_t r = read(fd, (char*)buf + total, n - total);
        if (r <= 0) return false;
        total += r;
    }
    return true;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: nusa-run <program.nbin>\n");
        return 1;
    }

    const char* path = argv[1];

    char resolved[4096];
    if (path[0] != '/') {
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd)) != nullptr) {
            snprintf(resolved, sizeof(resolved), "%s/%s", cwd, path);
            path = resolved;
        }
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "nusa-run: couldn't open '%s': %s\n", path, strerror(errno));
        return 1;
    }

    char header[9];
    memset(header, 0, sizeof(header));
    if (!read_exact(fd, header, 9)) {
        fprintf(stderr, "nusa-run: '%s': file too small\n", path);
        close(fd);
        return 1;
    }
    if (memcmp(header, NUSA_MAGIC, 6) != 0) {
        fprintf(stderr, "nusa-run: '%s': not a valid .nbin file\n", path);
        close(fd);
        return 1;
    }

    uint8_t  sym_count = (uint8_t)header[6];
    uint16_t bc_size   = (uint8_t)header[7] | ((uint16_t)(uint8_t)header[8] << 8);

    if (sym_count > NUSA_MAX_VARS) {
        fprintf(stderr, "nusa-run: '%s': corrupt header\n", path);
        close(fd);
        return 1;
    }

    std::vector<uint8_t> bytecode(bc_size);
    if (bc_size > 0 && !read_exact(fd, bytecode.data(), bc_size)) {
        fprintf(stderr, "nusa-run: '%s': truncated bytecode\n", path);
        close(fd);
        return 1;
    }

    close(fd);
    return NusaVM::run(bytecode, (int)sym_count, "nusa-run");
}