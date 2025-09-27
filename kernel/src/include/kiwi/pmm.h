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

#define E820_TYPE_RAM               1
#define E820_TYPE_RESERVED          2
#define E820_TYPE_ACPI_RECLAIMABLE  3
#define E820_TYPE_ACPI_NVS          4
#define E820_TYPE_BAD_MEMORY        5

#define E820_ACPI_FLAGS_VALID       1
#define E820_ACPI_FLAGS_NVS         2

/* hierarchy parameters */
#define PMM_FANOUT                  64  /* fits nicely in a machine word */
#define PMM_MAX_LEVELS              7   /* up to 16 PB (16384 TB) with 4 KB pages */

typedef struct E820Entry {
    u64 base;
    u64 length;
    u32 type;
    u32 acpi_flags;
}__attribute__((packed)) E820Entry;

typedef struct PhysicalMemory {
    u64 total_memory;
    u64 hardware_reserved_memory;
    u64 usable_memory;
    u64 used_memory;

    u8 *bitmap_start;
    u8 bitmap_layer_count;
    u64 bitmap_layer_bit_offsets[PMM_MAX_LEVELS];
    u64 bitmap_layer_bit_sizes[PMM_MAX_LEVELS];
} PhysicalMemory;

extern PhysicalMemory pmm;

void pmm_init(void);
uptr pmm_alloc_page(void);
void pmm_free_page(uptr page);
