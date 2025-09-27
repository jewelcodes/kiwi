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
#include <kiwi/arch/memory.h>
#include <kiwi/arch/atomic.h>
#include <string.h>

PhysicalMemory pmm;

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

static int pmm_bit_set(u8 *bitmap, u64 bit) {
    u64 *word = (u64 *)((uptr) bitmap + (bit / 64) * 8);
    u64 bit_offset = bit % 64;
    u64 old = *word;
    u64 new = old | (1ULL << bit_offset);
    return arch_cas64(word, old, new);
}

static int pmm_bit_clear(u8 *bitmap, u64 bit) {
    u64 *word = (u64 *)((uptr) bitmap + (bit / 64) * 8);
    u64 bit_offset = bit % 64;

    u64 old = *word;
    u64 new = old & ~(1ULL << bit_offset);
    return arch_cas64(word, old, new);
}

void pmm_init(void) {
    memset(&pmm, 0, sizeof(PhysicalMemory));
    E820Entry *map = (E820Entry *)(uptr)kiwi_boot_info.memory_map;

    int entries = kiwi_boot_info.memory_map_entries;

    u64 highest_addr = 0;

    debug_info("memory map (%d entries):", entries);
    for(int i = 0; i < entries; i++) {
        debug_info(" [0x%016llX, 0x%016llX]: %s (%d)",
            map[i].base,
            map[i].base + map[i].length - 1,
            pmm_type_to_str(map[i].type),
            map[i].type);

        if(map[i].base + map[i].length > highest_addr) {
            highest_addr = map[i].base + map[i].length;
        }

        pmm.total_memory += map[i].length;
        if(map[i].type == E820_TYPE_RAM) {
            pmm.usable_memory += map[i].length;
        } else {
            pmm.hardware_reserved_memory += map[i].length;
        }
    }

    debug_info("building pmm hierarchy...");

    pmm.bitmap_start = (u8 *) PAGE_ALIGN_UP(kiwi_boot_info.lowest_free_address);
    usize first_layer_size_bits = PAGE_ALIGN_UP(highest_addr) / PAGE_SIZE;
    if(first_layer_size_bits % PMM_FANOUT) {
        first_layer_size_bits += PMM_FANOUT - (first_layer_size_bits % PMM_FANOUT);
    }

    memset(pmm.bitmap_start, 0xFF, first_layer_size_bits / 8);

    // clear out the usable regions
    int linear_free_count = 0;
    for(int i = 0; i < entries; i++) {
        if(map[i].type != E820_TYPE_RAM) {
            continue;
        }

        u64 start = PAGE_ALIGN_UP(map[i].base);
        u64 end = PAGE_ALIGN_DOWN(map[i].base + map[i].length);
        u64 page_count = (end - start) / PAGE_SIZE;
        if(!page_count) {
            continue;
        }

        for(u64 p = 0; p < page_count; p++) {
            while(pmm_bit_clear(pmm.bitmap_start, (start / PAGE_SIZE) + p) == 0);
            linear_free_count++;
        }
    }

    debug_info(" layer 0: offset=0 size=%llu, %d/%d usable",
        first_layer_size_bits, linear_free_count, first_layer_size_bits);

    // build the hierarchical bitmap
    pmm.bitmap_layer_bit_offsets[0] = 0;
    pmm.bitmap_layer_bit_sizes[0] = first_layer_size_bits;
    pmm.bitmap_layer_count = 1;

    for(int i = 0; i < PMM_MAX_LEVELS - 1; i++) {
        if(pmm.bitmap_layer_bit_sizes[i] <= PMM_FANOUT) {
            break;
        }

        u64 prev_layer_offset = pmm.bitmap_layer_bit_offsets[i];
        u64 prev_layer_size = pmm.bitmap_layer_bit_sizes[i];
        u64 this_layer_offset = prev_layer_offset + prev_layer_size;
        if(this_layer_offset % PMM_FANOUT) {
            this_layer_offset += PMM_FANOUT - (this_layer_offset % PMM_FANOUT);
        }

        u64 this_layer_size = (prev_layer_size) / PMM_FANOUT;
        if((this_layer_size > PMM_FANOUT) && (this_layer_size % PMM_FANOUT)) {
            this_layer_size += PMM_FANOUT - (this_layer_size % PMM_FANOUT);
        }

        pmm.bitmap_layer_bit_offsets[i + 1] = this_layer_offset;
        pmm.bitmap_layer_bit_sizes[i + 1] = this_layer_size;
        pmm.bitmap_layer_count++;

        memset(pmm.bitmap_start + this_layer_offset / 8, 0xFF, this_layer_size / 8);

        u64 *prev_layer = (u64 *)((uptr)pmm.bitmap_start + prev_layer_offset / 8);

        int count = 0;
        for(int j = 0; j < prev_layer_size / PMM_FANOUT; j++) {
            u64 word = prev_layer[j];
            if(word != (u64) -1) {
                while(pmm_bit_clear(pmm.bitmap_start, this_layer_offset + j) == 0);
                count++;
            }
        }

        debug_info(" layer %d: offset=%llu size=%llu, %d/%d usable", 
            i + 1,
            this_layer_offset,
            this_layer_size,
            count,
            this_layer_size);
    }

    debug_info("total physical memory = %llu KB (%llu %sB)", 
        pmm.total_memory / 1024,
        pmm.total_memory > 0x80000000 ? pmm.total_memory / 0x40000000
            : pmm.total_memory / 0x100000,
        pmm.total_memory > 0x80000000 ? "G" : "M");
    debug_info("hardware-reserved memory = %llu KB", pmm.hardware_reserved_memory / 1024);
    debug_info("usable memory = %llu KB (%llu %sB)", 
        pmm.usable_memory / 1024,
        pmm.usable_memory > 0x80000000 ? pmm.usable_memory / 0x40000000
            : pmm.usable_memory / 0x100000,
        pmm.usable_memory > 0x80000000 ? "G" : "M");

    u64 overhead = (pmm.bitmap_layer_bit_offsets[pmm.bitmap_layer_count - 1]
        + pmm.bitmap_layer_bit_sizes[pmm.bitmap_layer_count - 1] + 7) / 8;
    debug_info("pmm overhead = %llu KB", overhead / 1024);

    for(;;);
}