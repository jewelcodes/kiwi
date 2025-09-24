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

#define ELF_64_BIT_WIDTH            2
#define ELF_LITTLE_ENDIAN           1
#define ELF_ARCH_X86_64             0x3E
#define ELF_TYPE_EXECUTABLE         2

#define ELF_PROGRAM_TYPE_NULL       0
#define ELF_PROGRAM_TYPE_LOAD       1

#define ELF_PROGRAM_FLAG_EXECUTABLE 0x01
#define ELF_PROGRAM_FLAG_WRITABLE   0x02
#define ELF_PROGRAM_FLAG_READABLE   0x04

typedef struct ELFHeader {
    u8 magic[4];
    u8 bit_width;
    u8 endianness;
    u8 header_version;
    u8 os_abi;
    u8 padding[8];
    u16 type;
    u16 arch;
    u32 version;
    u64 entry;
    u64 ph_offset;
    u64 sh_offset;
    u32 flags;
    u16 eh_size;
    u16 ph_entry_size;
    u16 ph_entry_count;
    u16 sh_entry_size;
    u16 sh_entry_count;
    u16 sh_str_index;
} __attribute__((packed)) ELFHeader;

typedef struct ELFProgramHeader {
    u32 type;
    u32 flags;
    u64 offset;
    u64 vaddr;
    u64 paddr;
    u64 file_size;
    u64 mem_size;
    u64 align;
} ELFProgramHeader;

int elf_load(const void *image, u64 *entry, u64 *highest);
