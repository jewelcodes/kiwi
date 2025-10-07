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

#include <kiwi/arch/apic.h>
#include <kiwi/structs/array.h>
#include <kiwi/vmm.h>
#include <kiwi/debug.h>
#include <stdlib.h>

static void *lapic = NULL;
static Array *lapics = NULL;

void lapic_write(u32 reg, u32 val) {
    u32 volatile *ptr = (u32 volatile *) ((uptr) lapic + reg);
    *ptr = val;
}

u32 lapic_read(u32 reg) {
    u32 volatile *ptr = (u32 volatile *) ((uptr) lapic + reg);
    return *ptr;
}

void lapic_register(MADTLocalAPIC *entry, int up) {
    LocalAPIC *lapic = malloc(sizeof(LocalAPIC));
    if(!lapic) {
        goto no_memory;
    }

    lapic->acpi_id = entry->acpi_id;
    lapic->apic_id = entry->apic_id;
    lapic->enabled = (entry->flags & MADT_LAPIC_FLAGS_ENABLED) ? 1 : 0;
    lapic->up = up;

    if(!lapics) {
        lapics = array_create();
    }

    if(!lapics) {
        goto no_memory;
    }

    if(array_push(lapics, (uptr) lapic)) {
        goto no_memory;
    }

    return;

no_memory:
    debug_error("failed to allocate memory for local APIC");
    for(;;);
}

void lapic_init(uptr mmio_base) {
    if(!lapic) {
        lapic = vmm_create_mmio(NULL, mmio_base, 1, VMM_PROT_READ | VMM_PROT_WRITE);
    }

    if(!lapic) {
        debug_error("failed to map local APIC MMIO");
        for(;;);
    }

    lapic_write(LAPIC_TASK_PRIORITY, 0);
    lapic_write(LAPIC_DESTINATION_FORMAT, lapic_read(LAPIC_DESTINATION_FORMAT) | 0xF0000000);
    lapic_write(LAPIC_SPURIOUS_INTERRUPT, LAPIC_SPURIOUS_VECTOR | LAPIC_SPURIOUS_ENABLE);
    lapic_write(LAPIC_LVT_ERROR, LAPIC_LVT_MASK);
    lapic_write(LAPIC_LVT_TIMER, LAPIC_LVT_MASK);
}
