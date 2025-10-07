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
#include <kiwi/arch/apic.h>
#include <kiwi/arch/smp.h>
#include <kiwi/arch/paging.h>
#include <kiwi/debug.h>
#include <kiwi/pmm.h>
#include <kiwi/vmm.h>
#include <string.h>
#include <stdlib.h>

#define AP_INITIAL_STACK_PAGES  8           /* 32 KB */

#define AP_ENTRY_POINT          0x1000
#define CR3_PTR                 0x2000
#define STACK_PTR               0x2008
#define ENTRY_POINT_PTR         0x2010

extern u8 ap_early_main[];
extern u8 ap_early_main_end[];

static int booted = 0;

void ap_main(void) {
    debug_info("CPU is up and running");

    booted = 1;
    arch_flush_cache();
    for(;;);
}

void smp_init(void) {
    if(lapics->count < 2) {
        return;
    }

    debug_info("attempt to start %u secondary cores...", lapics->count - 1);

    // temporarily map low memory so the AP can access it
    for(int i = 0; i < 8; i++) {
        arch_map_page(arch_get_cr3(), i * PAGE_SIZE, i * PAGE_SIZE,
            VMM_PROT_READ | VMM_PROT_WRITE);
    }

    u64 volatile *cr3_ptr = (u64 volatile *) CR3_PTR;
    *cr3_ptr = arch_get_cr3();

    u64 volatile *stack_ptr = (u64 volatile *) STACK_PTR;
    u64 volatile *entry_point_ptr = (u64 volatile *) ENTRY_POINT_PTR;

    usize ap_code_size = (usize)((uptr) &ap_early_main_end - (uptr) &ap_early_main);

    for(int i = 0; i < lapics->count; i++) {
        LocalAPIC *lapic = (LocalAPIC *) lapics->items[i];
        if(!lapic || !lapic->enabled || lapic->up) {
            continue;
        }

        debug_info("booting CPU with APIC ID %u...", lapic->apic_id);

        void *new_stack = calloc(AP_INITIAL_STACK_PAGES, PAGE_SIZE);
        if(!new_stack) {
            debug_error("failed to allocate stack");
            for(;;);
        }

        *stack_ptr = (u64) new_stack + (AP_INITIAL_STACK_PAGES * PAGE_SIZE);
        *entry_point_ptr = (u64) &ap_main;

        memcpy((void *) AP_ENTRY_POINT + ARCH_HHDM_BASE, &ap_early_main, ap_code_size);
        booted = 0;
        arch_flush_cache();

        lapic_write(LAPIC_INT_COMMAND_HIGH, lapic->apic_id << 24);
        lapic_write(LAPIC_INT_COMMAND_LOW, LAPIC_INT_COMMAND_INIT
            | LAPIC_INT_COMMAND_LEVEL_ASSERT | LAPIC_INT_COMMAND_TRIGGER_LEVEL);
        while(lapic_read(LAPIC_INT_COMMAND_LOW) & LAPIC_INT_COMMAND_DELIVERED);

        lapic_write(LAPIC_INT_COMMAND_HIGH, lapic->apic_id << 24);
        lapic_write(LAPIC_INT_COMMAND_LOW, LAPIC_INT_COMMAND_INIT);
        while(lapic_read(LAPIC_INT_COMMAND_LOW) & LAPIC_INT_COMMAND_DELIVERED);

        lapic_write(LAPIC_INT_COMMAND_HIGH, lapic->apic_id << 24);
        lapic_write(LAPIC_INT_COMMAND_LOW, LAPIC_INT_COMMAND_STARTUP
            | LAPIC_INT_COMMAND_TRIGGER_LEVEL | (AP_ENTRY_POINT >> 12));

        while(!booted) {
            arch_spin_backoff();
        }
    }

    for(int i = 0; i < 8; i++) {
        arch_unmap_page(arch_get_cr3(), i * PAGE_SIZE);
    }
}
