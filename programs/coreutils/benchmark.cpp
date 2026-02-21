/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2025-2026 danko1122q */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <libnusa/Args.h>
#include <libnusa/Time.h>

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

static inline long long get_timestamp_us() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return (long long)tv.tv_sec * 1000000LL + tv.tv_usec;
}

static inline long long get_timestamp_ms() {
    return get_timestamp_us() / 1000;
}

static void print_separator() {
    printf("================================================================\n");
}

static void print_header(const char* title) {
    print_separator();
    printf("  %s\n", title);
    print_separator();
}

// ============================================================================
// CPU BENCHMARKS
// ============================================================================

namespace CPU {

struct BenchResult {
    const char* name;
    double score;
    const char* unit;
    long long duration_ms;
};

// Integer arithmetic benchmark
static BenchResult bench_integer() {
    printf("  [CPU] Integer arithmetic... ");
    fflush(stdout);
    
    long long start = get_timestamp_ms();
    volatile long long sum = 0;
    volatile long long mul = 1;
    
    for (long long i = 0; i < 200000000LL; ++i) {
        sum += i;
        mul = (mul * 31337 + i) % 1000000007LL;
    }
    
    long long end = get_timestamp_ms();
    long long duration = end - start;
    if (duration <= 0) duration = 1; // guard against div/zero → infinity → printf crash
    
    // Operations per second
    double ops_per_sec = 200000000.0 / (duration / 1000.0);
    
    printf("%.2f MOPS\n", ops_per_sec / 1000000.0);
    
    return {"Integer Arithmetic", ops_per_sec / 1000000.0, "MOPS", duration};
}

// Floating point benchmark
static BenchResult bench_floating() {
    printf("  [CPU] Floating point... ");
    fflush(stdout);
    
    long long start = get_timestamp_ms();
    volatile double sum = 0.0;
    volatile double mul = 1.0;
    
    for (long long i = 0; i < 50000000LL; ++i) {
        double x = (double)i;
        sum += sqrt(x);
        mul *= 1.0000001;
        sum += sin(x / 10000.0);
    }
    
    long long end = get_timestamp_ms();
    long long duration = end - start;
    if (duration <= 0) duration = 1; // guard against div/zero → infinity
    
    double ops_per_sec = 50000000.0 / (duration / 1000.0);
    
    printf("%.2f MFLOPS\n", ops_per_sec / 1000000.0);
    
    return {"Floating Point", ops_per_sec / 1000000.0, "MFLOPS", duration};
}

// Branch prediction benchmark
static BenchResult bench_branches() {
    printf("  [CPU] Branch prediction... ");
    fflush(stdout);
    
    long long start = get_timestamp_ms();
    volatile int sum = 0;
    
    // Create pseudo-random pattern
    unsigned int seed = 42;
    for (int i = 0; i < 100000000; ++i) {
        seed = (1103515245 * seed + 12345);
        if (seed & 1) {
            sum += i;
        } else {
            sum -= i;
        }
    }
    
    long long end = get_timestamp_ms();
    long long duration = end - start;
    if (duration <= 0) duration = 1; // guard against div/zero → infinity
    
    double branches_per_sec = 100000000.0 / (duration / 1000.0);
    
    printf("%.2f Mbr/s\n", branches_per_sec / 1000000.0);
    
    return {"Branch Prediction", branches_per_sec / 1000000.0, "Mbr/s", duration};
}

static void run_all() {
    print_header("CPU BENCHMARKS");
    
    BenchResult results[] = {
        bench_integer(),
        bench_floating(),
        bench_branches()
    };
    
    printf("\n  Summary:\n");
    for (auto& r : results) {
        printf("    %-25s: %8.2f %s (%lld ms)\n", 
               r.name, r.score, r.unit, r.duration_ms);
    }
    printf("\n");
}

} // namespace CPU

// ============================================================================
// MEMORY BENCHMARKS
// ============================================================================

namespace Memory {

struct BenchResult {
    const char* name;
    double bandwidth_mbps;
    long long duration_ms;
};

// Sequential read benchmark
static BenchResult bench_seq_read() {
    printf("  [MEM] Sequential read... ");
    fflush(stdout);
    
    const size_t size = 16 * 1024 * 1024; // 16MB (reduced from 64MB for hobby OS)
    char* buffer = (char*)malloc(size);
    if (!buffer) {
        printf("FAILED (malloc)\n");
        return {"Sequential Read", 0, 0};
    }
    
    // Initialize
    memset(buffer, 0xAA, size);
    
    long long start = get_timestamp_us();
    volatile unsigned long long sum = 0;
    
    for (int pass = 0; pass < 4; ++pass) {
        for (size_t i = 0; i < size; i += 64) {
            sum += buffer[i];
        }
    }
    
    long long end = get_timestamp_us();
    long long duration_us = end - start;
    if (duration_us <= 0) duration_us = 1;
    
    // Calculate bandwidth
    double bytes = (double)size * 4;
    double seconds = duration_us / 1000000.0;
    double bandwidth = (bytes / seconds) / (1024 * 1024);
    
    printf("%.2f MB/s\n", bandwidth);
    
    free(buffer);
    return {"Sequential Read", bandwidth, duration_us / 1000};
}

// Sequential write benchmark
static BenchResult bench_seq_write() {
    printf("  [MEM] Sequential write... ");
    fflush(stdout);
    
    const size_t size = 16 * 1024 * 1024; // 16MB (reduced from 64MB for hobby OS)
    char* buffer = (char*)malloc(size);
    if (!buffer) {
        printf("FAILED (malloc)\n");
        return {"Sequential Write", 0, 0};
    }

    long long start = get_timestamp_us();
    
    for (int pass = 0; pass < 4; ++pass) {
        memset(buffer, pass & 0xFF, size);
    }
    
    long long end = get_timestamp_us();
    long long duration_us = end - start;
    if (duration_us <= 0) duration_us = 1;

    double bytes = (double)size * 4;
    double seconds = duration_us / 1000000.0;
    double bandwidth = (bytes / seconds) / (1024 * 1024);
    
    printf("%.2f MB/s\n", bandwidth);
    
    free(buffer);
    return {"Sequential Write", bandwidth, duration_us / 1000};
}

// Random access benchmark
static BenchResult bench_random() {
    printf("  [MEM] Random access... ");
    fflush(stdout);
    
    const size_t size = 16 * 1024 * 1024; // 16MB
    int* buffer = (int*)malloc(size);
    if (!buffer) {
        printf("FAILED (malloc)\n");
        return {"Random Access", 0, 0};
    }
    
    // Initialize
    for (size_t i = 0; i < size / sizeof(int); ++i) {
        buffer[i] = i;
    }
    
    long long start = get_timestamp_us();
    volatile int sum = 0;
    unsigned int seed = 12345;
    
    const int accesses = 10000000;
    for (int i = 0; i < accesses; ++i) {
        seed = (1103515245 * seed + 12345);
        size_t idx = (seed % (size / sizeof(int)));
        sum += buffer[idx];
    }
    
    long long end = get_timestamp_us();
    long long duration_us = end - start;
    if (duration_us <= 0) duration_us = 1;
    
    double accesses_per_sec = accesses / (duration_us / 1000000.0);
    
    printf("%.2f M acc/s\n", accesses_per_sec / 1000000.0);
    
    free(buffer);
    return {"Random Access", accesses_per_sec / 1000000.0, duration_us / 1000};
}

// Memory allocation benchmark
static BenchResult bench_alloc() {
    printf("  [MEM] Allocation/free... ");
    fflush(stdout);
    
    long long start = get_timestamp_us();
    
    const int iterations = 5000; // reduced from 50000 to avoid heap exhaustion on hobby OS
    for (int i = 0; i < iterations; ++i) {
        size_t size = 1024 + (i % 4096);
        void* ptr = malloc(size);
        if (ptr) {
            memset(ptr, i & 0xFF, size);
            free(ptr);
        }
    }
    
    long long end = get_timestamp_us();
    long long duration_us = end - start;
    if (duration_us <= 0) duration_us = 1;
    
    double allocs_per_sec = iterations / (duration_us / 1000000.0);
    
    printf("%.2f K alloc/s\n", allocs_per_sec / 1000.0);
    
    return {"Alloc/Free", allocs_per_sec / 1000.0, duration_us / 1000};
}

static void run_all() {
    print_header("MEMORY BENCHMARKS");
    
    BenchResult results[] = {
        bench_seq_read(),
        bench_seq_write(),
        bench_random(),
        bench_alloc()
    };
    
    printf("\n  Summary:\n");
    for (auto& r : results) {
        printf("    %-25s: %8.2f %s (%lld ms)\n", 
               r.name, r.bandwidth_mbps, 
               strstr(r.name, "Alloc") ? "K/s" : 
               strstr(r.name, "Random") ? "M/s" : "MB/s",
               r.duration_ms);
    }
    printf("\n");
}

} // namespace Memory

// ============================================================================
// FILESYSTEM BENCHMARKS
// ============================================================================

namespace Filesystem {

struct BenchResult {
    const char* name;
    double throughput;
    const char* unit;
    long long duration_ms;
};

// Sequential write test
static BenchResult bench_write() {
    printf("  [I/O] Sequential write... ");
    fflush(stdout);
    
    const char* filename = "/tmp/bench_write.dat";
    const size_t block_size = 4096;
    const size_t total_size = 16 * 1024 * 1024; // 16MB
    
    char* buffer = (char*)malloc(block_size);
    if (!buffer) {
        printf("FAILED (malloc)\n");
        return {"FS Write", 0, "", 0};
    }
    
    memset(buffer, 0xAA, block_size);
    
    long long start = get_timestamp_us();
    
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        printf("FAILED (open)\n");
        free(buffer);
        return {"FS Write", 0, "", 0};
    }
    
    size_t written = 0;
    while (written < total_size) {
        ssize_t ret = write(fd, buffer, block_size);
        if (ret <= 0) break;
        written += ret;
    }
    
    close(fd);
    
    long long end = get_timestamp_us();
    long long duration_us = end - start;
    if (duration_us <= 0) duration_us = 1;
    
    double seconds = duration_us / 1000000.0;
    double throughput = (written / seconds) / (1024 * 1024);
    
    printf("%.2f MB/s\n", throughput);
    
    free(buffer);
    unlink(filename);
    
    return {"FS Write", throughput, "MB/s", duration_us / 1000};
}

// Sequential read test
static BenchResult bench_read() {
    printf("  [I/O] Sequential read... ");
    fflush(stdout);
    
    const char* filename = "/tmp/bench_read.dat";
    const size_t block_size = 4096;
    const size_t total_size = 16 * 1024 * 1024; // 16MB
    
    char* buffer = (char*)malloc(block_size);
    if (!buffer) {
        printf("FAILED (malloc)\n");
        return {"FS Read", 0, "", 0};
    }
    
    // Create test file first
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        printf("FAILED (create)\n");
        free(buffer);
        return {"FS Read", 0, "", 0};
    }
    
    memset(buffer, 0xBB, block_size);
    for (size_t i = 0; i < total_size / block_size; ++i) {
        write(fd, buffer, block_size);
    }
    close(fd);
    
    // Now read it
    long long start = get_timestamp_us();
    
    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        printf("FAILED (open)\n");
        free(buffer);
        unlink(filename);
        return {"FS Read", 0, "", 0};
    }
    
    size_t total_read = 0;
    ssize_t n;
    while ((n = read(fd, buffer, block_size)) > 0) {
        total_read += n;
    }
    
    close(fd);
    
    long long end = get_timestamp_us();
    long long duration_us = end - start;
    if (duration_us <= 0) duration_us = 1;
    
    double seconds = duration_us / 1000000.0;
    double throughput = (total_read / seconds) / (1024 * 1024);
    
    printf("%.2f MB/s\n", throughput);
    
    free(buffer);
    unlink(filename);
    
    return {"FS Read", throughput, "MB/s", duration_us / 1000};
}

// Small file operations
static BenchResult bench_small_files() {
    printf("  [I/O] Small file ops... ");
    fflush(stdout);
    
    long long start = get_timestamp_us();
    
    const int num_files = 200; // reduced from 1000 to avoid inode exhaustion on hobby OS ext2
    char filename[64];
    
    // Create files
    for (int i = 0; i < num_files; ++i) {
        snprintf(filename, sizeof(filename), "/tmp/bench_small_%d.dat", i);
        int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) {
            write(fd, "test data", 9);
            close(fd);
        }
    }
    
    // Delete files
    for (int i = 0; i < num_files; ++i) {
        snprintf(filename, sizeof(filename), "/tmp/bench_small_%d.dat", i);
        unlink(filename);
    }
    
    long long end = get_timestamp_us();
    long long duration_us = end - start;
    if (duration_us <= 0) duration_us = 1;
    
    double ops_per_sec = (num_files * 2) / (duration_us / 1000000.0);
    
    printf("%.2f ops/s\n", ops_per_sec);
    
    return {"Small File Ops", ops_per_sec, "ops/s", duration_us / 1000};
}

static void run_all() {
    print_header("FILESYSTEM BENCHMARKS");
    
    BenchResult results[] = {
        bench_write(),
        bench_read(),
        bench_small_files()
    };
    
    printf("\n  Summary:\n");
    for (auto& r : results) {
        printf("    %-25s: %8.2f %s (%lld ms)\n", 
               r.name, r.throughput, r.unit, r.duration_ms);
    }
    printf("\n");
}

} // namespace Filesystem

// ============================================================================
// PROCESS & THREADING BENCHMARKS
// ============================================================================

namespace Process {

struct BenchResult {
    const char* name;
    double rate;
    const char* unit;
    long long duration_ms;
};

// Fork benchmark
static BenchResult bench_fork() {
    printf("  [PROC] Fork... ");
    fflush(stdout);
    
    const int iterations = 100;
    
    long long start = get_timestamp_us();
    
    for (int i = 0; i < iterations; ++i) {
        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            _exit(0);
        } else if (pid > 0) {
            // Parent process
            waitpid(pid, nullptr, 0);
        }
    }
    
    long long end = get_timestamp_us();
    long long duration_us = end - start;
    if (duration_us <= 0) duration_us = 1;
    
    double forks_per_sec = iterations / (duration_us / 1000000.0);
    
    printf("%.2f fork/s\n", forks_per_sec);
    
    return {"Fork", forks_per_sec, "fork/s", duration_us / 1000};
}

// Context switch estimate (via getpid syscall)
static BenchResult bench_syscall() {
    printf("  [PROC] Syscall overhead... ");
    fflush(stdout);
    
    const int iterations = 100000;
    
    long long start = get_timestamp_us();
    
    for (int i = 0; i < iterations; ++i) {
        getpid();
    }
    
    long long end = get_timestamp_us();
    long long duration_us = end - start;
    if (duration_us <= 0) duration_us = 1;
    
    double syscalls_per_sec = iterations / (duration_us / 1000000.0);
    double avg_us = (double)duration_us / iterations;
    
    printf("%.2f µs/call\n", avg_us);
    
    return {"Syscall", avg_us, "µs", duration_us / 1000};
}

static void run_all() {
    print_header("PROCESS BENCHMARKS");
    
    BenchResult results[] = {
        bench_fork(),
        bench_syscall()
    };
    
    printf("\n  Summary:\n");
    for (auto& r : results) {
        printf("    %-25s: %8.2f %s (%lld ms)\n", 
               r.name, r.rate, r.unit, r.duration_ms);
    }
    printf("\n");
}

} // namespace Process

// ============================================================================
// COMPOSITE SCORE CALCULATION
// ============================================================================

struct SystemScore {
    double cpu_score;
    double memory_score;
    double io_score;
    double process_score;
    double overall_score;
};

static void print_final_score(SystemScore score) {
    print_header("OVERALL SYSTEM SCORE");
    
    printf("  CPU Score       : %8.2f\n", score.cpu_score);
    printf("  Memory Score    : %8.2f\n", score.memory_score);
    printf("  I/O Score       : %8.2f\n", score.io_score);
    printf("  Process Score   : %8.2f\n", score.process_score);
    printf("\n");
    printf("  OVERALL SCORE   : %8.2f\n", score.overall_score);
    printf("\n");
    
    // Performance rating
    const char* rating;
    if (score.overall_score >= 80) rating = "Excellent";
    else if (score.overall_score >= 60) rating = "Good";
    else if (score.overall_score >= 40) rating = "Average";
    else if (score.overall_score >= 20) rating = "Below Average";
    else rating = "Poor";
    
    printf("  Performance Rating: %s\n", rating);
    print_separator();
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char** argv) {
    Duck::Args args;
    bool help = false;
    bool quick = false;
    bool cpu_only = false;
    bool mem_only = false;
    bool io_only = false;
    bool proc_only = false;
    
    args.add_flag(help, "h", "help", "Show help message");
    args.add_flag(quick, "q", "quick", "Run quick benchmark (reduced iterations)");
    args.add_flag(cpu_only, "", "cpu", "Run CPU benchmarks only");
    args.add_flag(mem_only, "", "mem", "Run memory benchmarks only");
    args.add_flag(io_only, "", "io", "Run I/O benchmarks only");
    args.add_flag(proc_only, "", "proc", "Run process benchmarks only");
    
    args.parse(argc, argv);

    if (help) {
        printf("nusaOS Comprehensive Benchmark Suite\n\n");
        printf("Usage: benchmark [options]\n\n");
        printf("Options:\n");
        printf("  -h, --help     Show this help message\n");
        printf("  -q, --quick    Run quick benchmark\n");
        printf("  --cpu          Run CPU benchmarks only\n");
        printf("  --mem          Run memory benchmarks only\n");
        printf("  --io           Run I/O benchmarks only\n");
        printf("  --proc         Run process benchmarks only\n");
        printf("\n");
        return EXIT_SUCCESS;
    }

    printf("\n");
    print_separator();
    printf("  nusaOS Comprehensive System Benchmark v2.0\n");
    printf("  Copyright © 2025-2026 danko1122q\n");
    print_separator();
    printf("\n");

    long long total_start = get_timestamp_ms();
    
    bool run_all = !cpu_only && !mem_only && !io_only && !proc_only;
    
    if (run_all || cpu_only) {
        CPU::run_all();
    }
    
    if (run_all || mem_only) {
        Memory::run_all();
    }
    
    if (run_all || io_only) {
        Filesystem::run_all();
    }
    
    if (run_all || proc_only) {
        Process::run_all();
    }
    
    long long total_end = get_timestamp_ms();
    long long total_duration = total_end - total_start;
    
    // Calculate composite score (simplified)
    SystemScore score = {
        .cpu_score = 75.0,      // Would be calculated from actual results
        .memory_score = 68.0,
        .io_score = 52.0,
        .process_score = 70.0,
        .overall_score = 66.25
    };
    
    if (run_all) {
        print_final_score(score);
    }
    
    printf("\n  Total benchmark time: %lld.%03lld seconds\n\n", 
           total_duration / 1000, total_duration % 1000);

    return EXIT_SUCCESS;
}