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

#define E820_MAX_ENTRIES            64
#define E820_TYPE_RAM               1
#define E820_TYPE_RESERVED          2
#define E820_TYPE_ACPI_RECLAIMABLE  3
#define E820_TYPE_ACPI_NVS          4
#define E820_TYPE_BAD_MEMORY        5

#define E820_ACPI_FLAGS_VALID       1
#define E820_ACPI_FLAGS_NVS         2

typedef struct E820Entry {
    u64 base;
    u64 length;
    u32 type;
    u32 acpi_flags;
}__attribute__((packed)) E820Entry;

int detect_memory(void);

extern E820Entry e820_map[];
extern int e820_entries;
extern u64 total_memory, total_usable_memory;
