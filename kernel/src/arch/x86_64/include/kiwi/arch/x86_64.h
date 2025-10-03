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

#pragma once

#include <kiwi/types.h>

#define GDT_ENTRIES             7
#define IDT_ENTRIES             256

#define GDT_NULL                0
#define GDT_KERNEL_CODE         1
#define GDT_KERNEL_DATA         2
#define GDT_USER_DATA           3
#define GDT_USER_CODE           4
#define GDT_TSS_LOW             5
#define GDT_TSS_HIGH            6

#define GDT_ACCESS_ACCESSED     0x01
#define GDT_ACCESS_WRITABLE     0x02
#define GDT_ACCESS_DC           0x04
#define GDT_ACCESS_EXEC         0x08
#define GDT_ACCESS_CODE_DATA    0x10
#define GDT_ACCESS_DPL0         0x00
#define GDT_ACCESS_DPL3         0x60
#define GDT_ACCESS_PRESENT      0x80

#define GDT_FLAGS_AVAILABLE     0x10
#define GDT_FLAGS_64_BIT        0x20
#define GDT_FLAGS_32_BIT        0x40
#define GDT_FLAGS_GRANULARITY   0x80

#define IDT_FLAGS_VALID         0x8000
#define IDT_FLAGS_INTERRUPT     0x0E00
#define IDT_FLAGS_TRAP          0x0F00
#define IDT_FLAGS_DPL0          0x0000
#define IDT_FLAGS_DPL3          0x6000

typedef struct GDTR {
    u16 limit;
    u64 base;
} __attribute__((packed)) GDTR;

typedef struct IDTR {
    u16 limit;
    u64 base;
} __attribute__((packed)) IDTR;

typedef struct GDTEntry {
    u16 limit_low;
    u16 base_low;
    u8 base_middle;
    u8 access;
    u8 flags_limit_high;
    u8 base_high;
} __attribute__((packed)) GDTEntry;

typedef struct IDTEntry {
    u16 offset_low;
    u16 segment;
    u16 flags;
    u16 offset_middle;
    u32 offset_high;
    u32 reserved;
} __attribute__((packed)) IDTEntry;

typedef struct TSS {
    u32 reserved1;
    u64 rsp0;
    u64 rsp1;
    u64 rsp2;
    u64 reserved2;
    u64 ist[7];
    u64 reserved3;
    u16 reserved4;
    u16 iomap_offset;
    u8 iomap[8192];
    u8 ones;
} __attribute__((packed)) TSS;

typedef struct ExceptionStackFrame {
    u64 r15;
    u64 r14;
    u64 r13;
    u64 r12;
    u64 r11;
    u64 r10;
    u64 r9;
    u64 r8;
    u64 rbp;
    u64 rdi;
    u64 rsi;
    u64 rdx;
    u64 rcx;
    u64 rbx;
    u64 rax;
    u64 error_code;
    u64 rip;
    u64 cs;
    u64 rflags;
    u64 rsp;
    u64 ss;
} __attribute__((packed)) ExceptionStackFrame;

extern GDTEntry gdt[GDT_ENTRIES];
extern IDTEntry idt[IDT_ENTRIES];

u64 arch_get_cr0(void);
u64 arch_get_cr2(void);
u64 arch_get_cr3(void);
u64 arch_get_cr4(void);
void arch_set_cr0(u64 value);
void arch_set_cr3(u64 value);
void arch_set_cr4(u64 value);
void arch_load_gdt(const GDTR *gdtr);
void arch_load_idt(const IDTR *idtr);
void arch_load_tss(u16 selector);
void arch_reload_code_segment(u16 selector);
void arch_reload_data_segments(u16 selector);
void arch_enable_irqs(void);
void arch_disable_irqs(void);
void arch_halt(void);
void arch_invlpg(uptr addr);
int arch_install_isr(u8 vector, uptr handler, u16 segment, int user);
