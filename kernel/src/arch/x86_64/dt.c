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
#include <string.h>

GDTEntry gdt[GDT_ENTRIES];
IDTEntry idt[IDT_ENTRIES];

void arch_dt_setup(void) {
    memset(&gdt, 0, sizeof(gdt));
    memset(&idt, 0, sizeof(idt));

    gdt[GDT_KERNEL_CODE].limit_low = 0xFFFF;
    gdt[GDT_KERNEL_CODE].flags_limit_high = GDT_FLAGS_64_BIT | GDT_FLAGS_GRANULARITY | 0x0F;
    gdt[GDT_KERNEL_CODE].access = GDT_ACCESS_PRESENT | GDT_ACCESS_CODE_DATA
        | GDT_ACCESS_EXEC | GDT_ACCESS_WRITABLE | GDT_ACCESS_DPL0;

    gdt[GDT_KERNEL_DATA].limit_low = 0xFFFF;
    gdt[GDT_KERNEL_DATA].flags_limit_high = GDT_FLAGS_GRANULARITY | 0x0F;
    gdt[GDT_KERNEL_DATA].access = GDT_ACCESS_PRESENT | GDT_ACCESS_CODE_DATA
        | GDT_ACCESS_WRITABLE | GDT_ACCESS_DPL0;

    gdt[GDT_USER_CODE].limit_low = 0xFFFF;
    gdt[GDT_USER_CODE].flags_limit_high = GDT_FLAGS_64_BIT | GDT_FLAGS_GRANULARITY | 0x0F;
    gdt[GDT_USER_CODE].access = GDT_ACCESS_PRESENT | GDT_ACCESS_CODE_DATA
        | GDT_ACCESS_EXEC | GDT_ACCESS_WRITABLE | GDT_ACCESS_DPL3;

    gdt[GDT_USER_DATA].limit_low = 0xFFFF;
    gdt[GDT_USER_DATA].flags_limit_high = GDT_FLAGS_GRANULARITY | 0x0F;
    gdt[GDT_USER_DATA].access = GDT_ACCESS_PRESENT | GDT_ACCESS_CODE_DATA
        | GDT_ACCESS_WRITABLE | GDT_ACCESS_DPL3;
    
    GDTR gdtr;
    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base = (u64)&gdt;
    arch_load_gdt(&gdtr);
    arch_reload_code_segment(GDT_KERNEL_CODE << 3);
    arch_reload_data_segments(GDT_KERNEL_DATA << 3);

    IDTR idtr;
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (u64)&idt;
    arch_load_idt(&idtr);
}

int arch_install_isr(u8 vector, uptr handler, u16 segment, int user) {
    if(vector >= IDT_ENTRIES) {
        debug_error("arch_install_isr: invalid vector 0x%02X", vector);
        return -1;
    }

    idt[vector].offset_low = handler & 0xFFFF;
    idt[vector].offset_middle = (handler >> 16) & 0xFFFF;
    idt[vector].offset_high = handler >> 32;
    idt[vector].segment = segment | (user ? 0x03 : 0x00);
    idt[vector].flags = IDT_FLAGS_VALID | IDT_FLAGS_INTERRUPT;
    idt[vector].reserved = 0;

    return 0;
}
