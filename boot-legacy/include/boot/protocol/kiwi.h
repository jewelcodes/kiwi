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
#include <boot/bios.h>

#define KIWI_BOOT_MAGIC                     0x6977696B /* "kiwi" */
#define KIWI_BOOT_REVISION                  1
#define KIWI_FIRMWARE_BIOS                  0x01
#define KIWI_FIRMWARE_UEFI                  0x02

#define KIWI_MEMORY_MAP_SOURCE_BIOS         0x01

typedef struct {
    u32 magic;
    u32 revision;
    u8 firmware_type;

    u64 initrd;
    u64 initrd_size;

    u64 memory_map;
    u64 lowest_free_address;
    u32 memory_map_entries;
    u8 memory_map_source;

    u64 acpi_rsdp;

    u64 video_memory;
    u64 framebuffer;
    u32 framebuffer_width;
    u32 framebuffer_height;
    u32 framebuffer_pitch;
    u8 framebuffer_bpp;

    /* BIOS-only */
    u8 bios_boot_disk;
    MBRPartition bios_boot_partition;

    /* TODO: UEFI */

    s8 command_line[512];
} __attribute__((packed)) KiwiBootInfo;

extern KiwiBootInfo boot_info;

int boot_kiwi(const char *command, const char *initrd);
