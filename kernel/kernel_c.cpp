// kernel_c.cpp - Wrapper C++ ke C
#include "kernel_c.h"
#include <kernel/kstd/kstdio.h>
#include <kernel/arch/Processor.h>
#include "IO.h"

extern "C" void kprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

extern "C" void disable_interrupts(void) {
    Processor::disable_interrupts();
}

extern "C" void enable_interrupts(void) {
    Processor::enable_interrupts();
}

extern "C" void halt_cpu(void) {
    while(true) {
        asm volatile("hlt");
    }
}

extern "C" void io_outb(uint16_t port, uint8_t val) {
    IO::outb(port, val);
}

extern "C" void io_outw(uint16_t port, uint16_t val) {
    IO::outw(port, val);
}

extern "C" uint8_t io_inb(uint16_t port) {
    return IO::inb(port);
}