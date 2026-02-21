// power.c - Kernel power management (C implementation)
#include "power.h"


// ============================================================================
// Definisi tipe sendiri (karena tidak bisa include C++ headers)
// ============================================================================

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef uint32_t size_t;

#ifndef NULL
#define NULL ((void*)0)
#endif

// ============================================================================
// Inline assembly untuk IO dan interrupts
// ============================================================================

static inline void cli(void) {
    __asm__ __volatile__("cli");
}

static inline void hlt(void) {
    __asm__ __volatile__("hlt");
}

static inline void io_barrier(void) {
    __asm__ __volatile__("" ::: "memory");
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ __volatile__("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// ============================================================================
// Constants
// ============================================================================

#define KBD_CTRL_PORT       0x64
#define KBD_DATA_PORT       0x60
#define KBD_RESET_CMD       0xFE
#define KBD_STATUS_IBF      0x02
#define KBD_STATUS_OBF      0x01

#define QEMU_SHUTDOWN_PORT  0x604
#define BOCHS_SHUTDOWN_PORT 0xB004
#define VBOX_SHUTDOWN_PORT  0x4004
#define SHUTDOWN_MAGIC      0x2000
#define VBOX_MAGIC          0x3400

// ============================================================================
// Helper functions
// ============================================================================

static void small_delay(void) {
    for(volatile int i = 0; i < 1000; i++);
}

static void flush_keyboard_buffer(void) {
    uint8_t status;
    int retries = 0;
    
    while(((status = inb(KBD_CTRL_PORT)) & KBD_STATUS_OBF) && retries < 1000) {
        inb(KBD_DATA_PORT);
        small_delay();
        retries++;
    }
}

static int wait_keyboard_ready(uint32_t max_retries) {
    for(uint32_t i = 0; i < max_retries; i++) {
        if(!(inb(KBD_CTRL_PORT) & KBD_STATUS_IBF)) {
            return 1;
        }
        small_delay();
    }
    return 0;
}

// ============================================================================
// VGA text mode output (sederhana, tidak pakai printf)
// ============================================================================

static volatile char* vga_buffer = (volatile char*)0xB8000;
static uint16_t vga_pos = 0;

static void vga_clear_line(void) {
    uint16_t line_start = (vga_pos / 80) * 80;
    for(int i = 0; i < 80; i++) {
        vga_buffer[(line_start + i) * 2] = ' ';
        vga_buffer[(line_start + i) * 2 + 1] = 0x07;
    }
}

static void kprint(const char* str) {
    while(*str) {
        if(*str == '\n') {
            vga_pos = (vga_pos / 80 + 1) * 80;
            if(vga_pos >= 80 * 25) vga_pos = 0;  // Scroll simple
        } else {
            vga_buffer[vga_pos * 2] = *str;
            vga_buffer[vga_pos * 2 + 1] = 0x07;  // Light gray on black
            vga_pos++;
            if(vga_pos >= 80 * 25) vga_pos = 0;
        }
        str++;
    }
}

// ============================================================================
// Public API
// ============================================================================

void power_reboot(void) {
    kprint("[Power] System reboot initiated...\n");
    
    cli();
    io_barrier();
    
    flush_keyboard_buffer();
    
    if(!wait_keyboard_ready(10000)) {
        kprint("[Power] Warning: Keyboard controller timeout\n");
    }
    
    outb(KBD_CTRL_PORT, KBD_RESET_CMD);
    io_barrier();
    small_delay();
    
    kprint("[Power] Soft reset failed, triggering triple fault...\n");
    
    // Triple fault: load null IDT then trigger interrupt
    struct { uint16_t limit; uint32_t base; } __attribute__((packed)) null_idt = {0, 0};
    __asm__ __volatile__("lidt %0" :: "m"(null_idt));
    __asm__ __volatile__("int $0xFF");
    
    __builtin_unreachable();
}

void power_shutdown(void) {
    kprint("[Power] System shutdown initiated...\n");
    
    cli();
    io_barrier();
    
    outw(QEMU_SHUTDOWN_PORT, SHUTDOWN_MAGIC);
    io_barrier();
    small_delay();
    
    outw(BOCHS_SHUTDOWN_PORT, SHUTDOWN_MAGIC);
    io_barrier();
    small_delay();
    
    outw(VBOX_SHUTDOWN_PORT, VBOX_MAGIC);
    io_barrier();
    small_delay();
    
    kprint("[Power] Shutdown not supported by hardware, halting CPU...\n");
    kprint("[Power] You can safely power off the system now.\n");
    
    while(1) {
        cli();
        hlt();
    }
}