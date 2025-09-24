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

#include <boot/elf.h>
#include <stdio.h>
#include <string.h>

int elf_load(const void *image, u64 *entry, u64 *highest) {
    ELFHeader *header = (ELFHeader *) image;
    if(header->magic[0] != 0x7F || header->magic[1] != 'E'
        || header->magic[2] != 'L' || header->magic[3] != 'F') {
        return -1;
    }

    if(header->bit_width != ELF_64_BIT_WIDTH
        || header->endianness != ELF_LITTLE_ENDIAN
        || header->arch != ELF_ARCH_X86_64) {
        return -1;
    }

    if(header->type != ELF_TYPE_EXECUTABLE || header->ph_entry_count == 0) {
        return -1;
    }

    u64 ph_offset = header->ph_offset;
    u16 ph_entry_size = header->ph_entry_size;
    u16 ph_entry_count = header->ph_entry_count;
    u64 max_addr = 0;

    for(int i = 0; i < ph_entry_count; i++) {
        ELFProgramHeader *ph = (ELFProgramHeader *) (ph_offset + (u8 *) image + (i * ph_entry_size));
        if(ph->type != ELF_PROGRAM_TYPE_LOAD) {
            continue;
        }

        if(ph->vaddr + ph->mem_size > max_addr) {
            max_addr = ph->vaddr + ph->mem_size;
        }

        u32 ptr = ph->paddr;
        memcpy((void *) ptr, (u8 *) image + ph->offset, ph->file_size);
        if(ph->mem_size > ph->file_size) {
            memset((u8 *) ptr + ph->file_size, 0, ph->mem_size - ph->file_size);
        }
    }

    *entry = header->entry;
    *highest = max_addr;
    return 0;
}
