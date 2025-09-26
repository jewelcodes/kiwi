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

#include <kiwi/debug.h>
#include <kiwi/pmm.h>
#include <kiwi/boot.h>

static const char *pmm_type_to_str(int type) {
    switch(type) {
        case E820_TYPE_RAM: return "RAM";
        case E820_TYPE_RESERVED: return "reserved";
        case E820_TYPE_ACPI_RECLAIMABLE: return "ACPI reclaimable";
        case E820_TYPE_ACPI_NVS: return "ACPI NVS";
        case E820_TYPE_BAD_MEMORY: return "bad memory";
        default: return "unknown";
    }
}

void pmm_init(void) {
    E820Entry *map = (E820Entry *)(uptr)kiwi_boot_info.memory_map;

    int entries = kiwi_boot_info.memory_map_entries;

    u64 highest_addr = 0;
    u64 highest_usable_addr = 0;

    debug_info("memory map (%d entries):", entries);
    for(int i = 0; i < entries; i++) {
        debug_info(" 0x%016llX - 0x%016llX: %s (%d)",
            map[i].base,
            map[i].base + map[i].length - 1,
            pmm_type_to_str(map[i].type),
            map[i].type);

        if(map[i].base + map[i].length > highest_addr) {
            highest_addr = map[i].base + map[i].length;

            if(map[i].type == E820_TYPE_RAM) {
                highest_usable_addr = map[i].base + map[i].length;
            }
        }
    }

    debug_info("highest address: 0x%016llX", highest_addr - 1);
    debug_info("highest usable address: 0x%016llX", highest_usable_addr - 1);
}