/*
 * kiwi - general-purpose high-performance operating system
 * 
 * Copyright (c) 2025 Omar Elghoul
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <kiwi/arch/x86_64.h>
#include <kiwi/debug.h>

static const char *exceptions[] = {
    "divide error",                         // 0x00
    "debug exception",                      // 0x01
    "non-maskable interrupt",               // 0x02
    "breakpoint",                           // 0x03
    "overflow",                             // 0x04
    "boundary range exceeded",              // 0x05
    "undefined opcode",                     // 0x06
    "device not present",                   // 0x07
    "double fault",                         // 0x08
    NULL,                                   // 0x09
    "invalid TSS",                          // 0x0A
    "data segment exception",               // 0x0B
    "stack segment exception",              // 0x0C
    "general protection fault",             // 0x0D
    "page fault",                           // 0x0E
    NULL,                                   // 0x0F
    "math fault",                           // 0x10
    "alignment exception",                  // 0x11
    "machine check fail",                   // 0x12
    "extended math fault",                  // 0x13
    "virtualization fault",                 // 0x14
    "control protection fault",             // 0x15
    NULL,                                   // 0x16
    NULL,                                   // 0x17
    NULL,                                   // 0x18
    NULL,                                   // 0x19
    NULL,                                   // 0x1A
    NULL,                                   // 0x1B
    "hypervisor injection exception",       // 0x1C
    "VMM communication exception",          // 0x1D
    "security exception",                   // 0x1E
    NULL                                    // 0x1F
};

void isr0_handler(void);
void isr1_handler(void);
void isr2_handler(void);
void isr3_handler(void);
void isr4_handler(void);
void isr5_handler(void);
void isr6_handler(void);
void isr7_handler(void);
void isr8_handler(void);
void isr9_handler(void);
void isr10_handler(void);
void isr11_handler(void);
void isr12_handler(void);
void isr13_handler(void);
void isr14_handler(void);
void isr15_handler(void);
void isr16_handler(void);
void isr17_handler(void);
void isr18_handler(void);
void isr19_handler(void);
void isr20_handler(void);
void isr21_handler(void);
void isr22_handler(void);
void isr23_handler(void);
void isr24_handler(void);
void isr25_handler(void);
void isr26_handler(void);
void isr27_handler(void);
void isr28_handler(void);
void isr29_handler(void);
void isr30_handler(void);
void isr31_handler(void);

void arch_exceptions_setup(void) {
    debug_info("setting up exception handlers");

    arch_install_isr(0x00, (uptr) isr0_handler, GDT_KERNEL_CODE << 3, 0);
    arch_install_isr(0x01, (uptr) isr1_handler, GDT_KERNEL_CODE << 3, 0);
    arch_install_isr(0x02, (uptr) isr2_handler, GDT_KERNEL_CODE << 3, 0);
    arch_install_isr(0x03, (uptr) isr3_handler, GDT_KERNEL_CODE << 3, 0);
    arch_install_isr(0x04, (uptr) isr4_handler, GDT_KERNEL_CODE << 3, 0);
    arch_install_isr(0x05, (uptr) isr5_handler, GDT_KERNEL_CODE << 3, 0);
    arch_install_isr(0x06, (uptr) isr6_handler, GDT_KERNEL_CODE << 3, 0);
    arch_install_isr(0x07, (uptr) isr7_handler, GDT_KERNEL_CODE << 3, 0);
    arch_install_isr(0x08, (uptr) isr8_handler, GDT_KERNEL_CODE << 3, 0);
    arch_install_isr(0x09, (uptr) isr9_handler, GDT_KERNEL_CODE << 3, 0);
    arch_install_isr(0x0A, (uptr) isr10_handler, GDT_KERNEL_CODE << 3, 0);
    arch_install_isr(0x0B, (uptr) isr11_handler, GDT_KERNEL_CODE << 3, 0);
    arch_install_isr(0x0C, (uptr) isr12_handler, GDT_KERNEL_CODE << 3, 0);
    arch_install_isr(0x0D, (uptr) isr13_handler, GDT_KERNEL_CODE << 3, 0);
    arch_install_isr(0x0E, (uptr) isr14_handler, GDT_KERNEL_CODE << 3, 0);
    arch_install_isr(0x0F, (uptr) isr15_handler, GDT_KERNEL_CODE << 3, 0);
    arch_install_isr(0x10, (uptr) isr16_handler, GDT_KERNEL_CODE << 3, 0);
    arch_install_isr(0x11, (uptr) isr17_handler, GDT_KERNEL_CODE << 3, 0);
    arch_install_isr(0x12, (uptr) isr18_handler, GDT_KERNEL_CODE << 3, 0);
    arch_install_isr(0x13, (uptr) isr19_handler, GDT_KERNEL_CODE << 3, 0);
    arch_install_isr(0x14, (uptr) isr20_handler, GDT_KERNEL_CODE << 3, 0);
    arch_install_isr(0x15, (uptr) isr21_handler, GDT_KERNEL_CODE << 3, 0);
    arch_install_isr(0x16, (uptr) isr22_handler, GDT_KERNEL_CODE << 3, 0);
    arch_install_isr(0x17, (uptr) isr23_handler, GDT_KERNEL_CODE << 3, 0);
    arch_install_isr(0x18, (uptr) isr24_handler, GDT_KERNEL_CODE << 3, 0);
    arch_install_isr(0x19, (uptr) isr25_handler, GDT_KERNEL_CODE << 3, 0);
    arch_install_isr(0x1A, (uptr) isr26_handler, GDT_KERNEL_CODE << 3, 0);
    arch_install_isr(0x1B, (uptr) isr27_handler, GDT_KERNEL_CODE << 3, 0);
    arch_install_isr(0x1C, (uptr) isr28_handler, GDT_KERNEL_CODE << 3, 0);
    arch_install_isr(0x1D, (uptr) isr29_handler, GDT_KERNEL_CODE << 3, 0);
    arch_install_isr(0x1E, (uptr) isr30_handler, GDT_KERNEL_CODE << 3, 0);
    arch_install_isr(0x1F, (uptr) isr31_handler, GDT_KERNEL_CODE << 3, 0);
}

void arch_exception_handler(u64 vector, u64 error_code, uptr state) {
    ExceptionStackFrame *frame = (ExceptionStackFrame *) state;
    const char *message = (vector < 32 && exceptions[vector])
        ? exceptions[vector] : "undefined exception";

    debug_error("exception %u @ 0x%llX: %s (0x%llX)",
        vector, frame->rip, message, error_code);

    for(;;);
}