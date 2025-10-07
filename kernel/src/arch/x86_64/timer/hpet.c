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

#include <kiwi/arch/hpet.h>
#include <kiwi/debug.h>
#include <kiwi/vmm.h>

static void *hpet_mmio = NULL;
static u64 hpet_frequency_hz = 0;
static int hpet_timer_count = 0;

static void hpet_write(u64 offset, u64 value) {
    u64 volatile *ptr = (u64 volatile *)((uptr) hpet_mmio + offset);
    *ptr = value;
}

static u64 hpet_read(u64 offset) {
    u64 volatile *ptr = (u64 volatile *)((uptr) hpet_mmio + offset);
    return *ptr;
}

static void hpet_reset_timer(int index) {
    if(index < 0 || index >= hpet_timer_count) {
        return;
    }

    u64 timer_config = hpet_read(HPET_TIMER_CONFIG_CAP + index * HPET_TIMER_STRIDE);
    timer_config &= ~(HPET_TIMER_CONFIG_CAP_ENABLE | HPET_TIMER_CONFIG_CAP_PERIODIC);
    hpet_write(HPET_TIMER_CONFIG_CAP + index * HPET_TIMER_STRIDE, timer_config);
    hpet_write(HPET_TIMER_COMPARATOR + index * HPET_TIMER_STRIDE, 0);
}

u64 hpet_frequency(void) {
    return hpet_frequency_hz;
}

void hpet_block(u64 ns) {
    u64 start = hpet_read(HPET_MAIN_COUNTER);
    u64 ticks = (hpet_frequency_hz * ns) / 1000000000ULL;
    while((hpet_read(HPET_MAIN_COUNTER) - start) < ticks) {
        /* TODO: yield CPU time after working on the scheduler */
    }
}

int hpet_init(void) {
    HPETTable *hpet_table = (HPETTable *) acpi_find_table("HPET", 0);
    if(!hpet_table) {
        return -1;
    }

    if(hpet_table->base_address.address_space_id != ACPI_MEMORY_SPACE) {
        debug_error("HPET is not in memory-mapped I/O space");
        return -1;
    }

    debug_info("HPET @ 0x%llX", hpet_table->base_address.address);
    hpet_mmio = vmm_create_mmio(NULL, hpet_table->base_address.address, 1,
        VMM_PROT_READ | VMM_PROT_WRITE);
    if(!hpet_mmio) {
        debug_error("failed to map HPET MMIO");
        return -1;
    }

    hpet_write(HPET_GENERAL_CONFIG,
        hpet_read(HPET_GENERAL_CONFIG) & ~HPET_GENERAL_CONFIG_ENABLE);

    u64 cap = hpet_read(HPET_GENERAL_CAP);
    u64 period_fs = HPET_GENERAL_CAP_COUNTER_PERIOD(cap);
    hpet_frequency_hz = 1000000000000000ULL / period_fs;
    hpet_timer_count = HPET_GENERAL_CAP_TIMER_COUNT(cap) + 1;

    debug_info("%d timers at frequency %llu MHz", hpet_timer_count, hpet_frequency_hz / 1000000);

    hpet_write(HPET_MAIN_COUNTER, 0);
    for(int i = 0; i < hpet_timer_count; i++) {
        hpet_reset_timer(i);
    }

    hpet_write(HPET_GENERAL_CONFIG,
        hpet_read(HPET_GENERAL_CONFIG) | HPET_GENERAL_CONFIG_ENABLE);
    return 0;
}