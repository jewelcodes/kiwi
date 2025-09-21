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

#include <boot/bios.h>
#include <boot/memory.h>
#include <stdio.h>
#include <string.h>

E820Entry e820_map[E820_MAX_ENTRIES];
int e820_entries = 0;
u64 total_memory, total_usable_memory;

static Registers memory_regs;

int detect_memory(void) {
    e820_entries = 0;
    memset(e820_map, 0, sizeof(e820_map));
    total_memory = 0;
    total_usable_memory = 0;

    memory_regs.ebx = 0;

    while(e820_entries < E820_MAX_ENTRIES) {
        memory_regs.eax = 0xE820;
        memory_regs.ecx = sizeof(E820Entry);
        memory_regs.edx = 0x534D4150; // 'SMAP'
        memory_regs.edi = (u32)(usize)&e820_map[e820_entries];
        memory_regs.es = 0; // segment is 0 in real mode

        bios_int(0x15, &memory_regs);

        if(memory_regs.eflags & 1) break;
        if(memory_regs.eax != 0x534D4150) break;
        if(memory_regs.ecx < 20) break;

        if(memory_regs.ecx < sizeof(E820Entry)) {
            e820_map[e820_entries].acpi_flags = E820_ACPI_FLAGS_VALID;
        }

        if((u32) e820_map[e820_entries].length == 0) {
            goto next;
        }

        if(e820_map[e820_entries].acpi_flags & E820_ACPI_FLAGS_VALID) {
            total_memory += e820_map[e820_entries].length;
            if(e820_map[e820_entries].type == E820_TYPE_RAM) {
                total_usable_memory += e820_map[e820_entries].length;
            }
        }

        e820_entries++;
    next:
        if(!memory_regs.ebx) break;
    }

    if(!e820_entries) {
        printf("Failed to detect memory.\n");
        for(;;);
    }

    u64 memory_mb = total_usable_memory / (1024 * 1024);
    if(memory_mb < 16) {
        printf("Not enough memory detected (%llu MB).\nKiwi needs at least 16 MB.\n", memory_mb);
        for(;;);
    }

    return 0;
}
