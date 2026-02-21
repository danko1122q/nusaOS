// kernel_c.h
#ifndef KERNEL_C_H
#define KERNEL_C_H

#include <kernel/kstd/kstddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void kprintf(const char* fmt, ...);
void disable_interrupts(void);
void enable_interrupts(void);
void halt_cpu(void);
void io_outb(uint16_t port, uint8_t val);
void io_outw(uint16_t port, uint16_t val);
uint8_t io_inb(uint16_t port);

#ifdef __cplusplus
}
#endif

#endif