/*
 * kiwi - general-purpose high-performance operating system
 * 
 * Copyright (c) 2026 Omar Elghoul
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
#include <kiwi/boot.h>

static int mtrr_inited = 0;

static const char *type_to_string(u8 type) {
    switch(type) {
    case MTRR_TYPE_UNCACHABLE:
        return "uncachable";
    case MTRR_TYPE_WRITE_COMBINE:
        return "write-combine";
    case MTRR_TYPE_WRITE_THROUGH:
        return "write-through";
    case MTRR_TYPE_WRITE_PROTECTED:
        return "write-protected";
    case MTRR_TYPE_WRITEBACK:
        return "writeback";
    default:
        return "unknown type";
    }
}

static inline int physical_address_width(void) {
    CPUIDRegisters cpuid = { 0 };
    arch_read_cpuid(0x80000008, &cpuid);
    return cpuid.eax & 0xFF;
}

static inline u64 physical_address_mask(void) {
    int width = physical_address_width();
    return (width >= 64)
        ? 0xFFFFFFFFFFFFFFFFULL
        : ((1ULL << width) - 1);
}

int mtrr_get_range(int index, u8 *type_ptr, u64 *base_ptr, u64 *size_ptr) {
    u64 base = arch_read_msr(MSR_MTRR_REGION_BASE(index));
    u64 mask = arch_read_msr(MSR_MTRR_REGION_MASK(index));
    u64 size;
    u8 type;

    if(!(mask & MTRR_MASK_VALID))
        return -1;

    type = base & 0xFF;
    base &= physical_address_mask() & 0xFFFFFFFFFFFFF000ULL;
    mask &= physical_address_mask() & 0xFFFFFFFFFFFFF000ULL;
    size = (~mask & physical_address_mask()) + 1;

    if(base_ptr)
        *base_ptr = base;
    if(size_ptr)
        *size_ptr = size;
    if(type_ptr)
        *type_ptr = type;
    return 0;
}

int mtrr_set_range(int index, u8 type, u64 base, u64 size) {
    u64 mask;

    if(base % size != 0 || size & (size - 1) || !type_to_string(type))
        return -1;

    base &= physical_address_mask() & 0xFFFFFFFFFFFFF000ULL;
    mask = ~(size - 1) & physical_address_mask() & 0xFFFFFFFFFFFFF000ULL;
    arch_write_msr(MSR_MTRR_REGION_BASE(index), base | type);
    arch_write_msr(MSR_MTRR_REGION_MASK(index), mask | MTRR_MASK_VALID);
    return 0;
}

static inline u64 round_up_to_pow2(u64 x) {
    if(x <= 1)
        return 1;

    x--;

    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> 32;
    return x + 1;
}

static inline u8 get_variable_range_count(void) {
    return arch_read_msr(MSR_MTRR_CAP) & 0xFF;
}

static inline u8 find_free_mtrr(void) {
    u8 count = get_variable_range_count();
    u8 type;
    u64 base, size;

    for(u8 i = 0; i < count; i++) {
        if(mtrr_get_range(i, &type, &base, &size))
            return i;
    }

    return 0xFF;
}

static void set_framebuffer_wc(void) {
    u64 fb_base = kiwi_boot_info.framebuffer;
    u64 fb_size = round_up_to_pow2(kiwi_boot_info.framebuffer_height
        * kiwi_boot_info.framebuffer_pitch);
    u64 fb_end = fb_base + fb_size - 1;
    u8 count = get_variable_range_count();
    u8 type, mtrr_index;
    u64 base, end, size;

    for(u8 i = 0; i < count; i++) {
        if(!mtrr_get_range(i, &type, &base, &size)) {
            end = base + size - 1;
            if(fb_base >= base && fb_end <= end) {
                if(type == MTRR_TYPE_WRITE_COMBINE) {
                    return;
                } else {
                    mtrr_index = i;
                    goto set_mtrr;
                }
            }
        }
    }

    mtrr_index = find_free_mtrr();
    if(mtrr_index == 0xFF) {
        debug_warn("no free MTRR found to set framebuffer WC");
        return;
    }

set_mtrr:
    if(mtrr_set_range(mtrr_index, MTRR_TYPE_WRITE_COMBINE, fb_base, fb_size))
        debug_warn("failed to set MTRR #%u for framebuffer", mtrr_index);
}

void mtrr_init(void) {
    CPUIDRegisters cpuid = { 0 };
    u8 variable_range_count, type;
    u64 cap, base, size, default_type;

    arch_read_cpuid(1, &cpuid);
    if(!(cpuid.edx & (1 << 12)))
        return;     /* MTRR not supported */

    cap = arch_read_msr(MSR_MTRR_CAP);
    default_type = arch_read_msr(MSR_MTRR_DEF_TYPE);
    if((default_type & 0xFF) != MTRR_TYPE_WRITEBACK
        || !(default_type & MTRR_DEF_TYPE_ENABLED)) {
        default_type &= 0xFF;
        default_type |= MTRR_DEF_TYPE_ENABLED | MTRR_TYPE_WRITEBACK;
        arch_write_msr(MSR_MTRR_DEF_TYPE, default_type);
        debug_info("set default MTRR type to: %s (%u)",
            type_to_string(default_type & 0xFF), default_type & 0xFF);
    }

    if(!mtrr_inited) {
        variable_range_count = get_variable_range_count();
        if(variable_range_count)
            debug_info("MTRR variable range count: %u", variable_range_count);

        for(u8 i = 0; i < variable_range_count; i++) {
            if(mtrr_get_range(i, &type, &base, &size) == 0) {
                debug_info(" [0x%016llX, 0x%016llX]: %s (%u)",
                    base, base + size - 1, type_to_string(type), type);
            }
        }
    }

    if(cap & MTRR_CAP_WRITE_COMBINE)
        set_framebuffer_wc();

    mtrr_inited = 1;
}