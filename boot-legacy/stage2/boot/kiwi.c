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

#include <boot/protocol/kiwi.h>
#include <boot/fs.h>
#include <boot/output.h>
#include <boot/memory.h>
#include <boot/acpi.h>
#include <boot/bios.h>
#include <boot/input.h>
#include <boot/elf.h>
#include <boot/mode.h>
#include <stdio.h>
#include <string.h>

#define KIWI_FILE_BUFFER                0x400000 /* 4 MB */

KiwiBootInfo kiwi_boot_info;

int boot_kiwi(const char *command, const char *initrd) {
    if(!command || !strlen(command)) {
        return -1;
    }

    display.bg = palette[BLACK];
    display.fg = palette[LIGHT_GREEN];
    clear_screen();

    printf("Booting with command line %s\n\n", command);
    display.fg = palette[LIGHT_GRAY];

    kiwi_boot_info.magic = KIWI_BOOT_MAGIC;
    kiwi_boot_info.revision = KIWI_BOOT_REVISION;
    kiwi_boot_info.firmware_type = KIWI_FIRMWARE_BIOS;
    kiwi_boot_info.initrd = 0; // TODO
    kiwi_boot_info.initrd_size = 0;
    kiwi_boot_info.memory_map = (u32) e820_map;
    kiwi_boot_info.memory_map_entries = e820_entries;
    kiwi_boot_info.memory_map_source = KIWI_MEMORY_MAP_SOURCE_BIOS;
    kiwi_boot_info.acpi_rsdp = (u32) rsdp;
    kiwi_boot_info.framebuffer = display.current_mode->framebuffer;
    kiwi_boot_info.framebuffer_width = display.current_mode->width;
    kiwi_boot_info.framebuffer_height = display.current_mode->height;
    kiwi_boot_info.framebuffer_pitch = display.current_mode->pitch;
    kiwi_boot_info.framebuffer_bpp = display.current_mode->bpp;
    kiwi_boot_info.video_memory = video_memory;
    kiwi_boot_info.bios_boot_disk = bios_boot_info.boot_disk;
    memcpy(&kiwi_boot_info.bios_boot_partition, &bios_boot_info.boot_partition,
        sizeof(MBRPartition));
    strcpy((char *) kiwi_boot_info.command_line, command);

    char kernel_path[128];
    for(int i = 0; i < sizeof(kernel_path); i++) {
        if(!command[i] || command[i] == ' ' || command[i] == '\n') {
            kernel_path[i] = 0;
            break;
        }
        kernel_path[i] = command[i];
    }

    printf(" Loading kernel %s...\n", kernel_path);

    if(!load_file(kernel_path, (void *) KIWI_FILE_BUFFER, (usize) -1)) {
        display.fg = palette[LIGHT_RED];
        printf(" Couldn't load kernel binary. Press any key to go back.\n");
        input_read(NULL);
        return -1;
    }

    u64 entry, highest;
    if(elf_load((void *) KIWI_FILE_BUFFER, &entry, &highest) < 0) {
        display.fg = palette[LIGHT_RED];
        printf(" Invalid kernel binary. Press any key to go back.\n");
        input_read(NULL);
        return -1;
    }

    // TODO: initrd

    if(highest & (PAGE_SIZE - 1)) {
        highest = (highest & ~(PAGE_SIZE - 1)) + PAGE_SIZE;
    }

    // create the 64-bit page tables
    u64 *pml4 = (u64 *) ((u32) highest & 0x7FFFFFFF);
    u64 *pdp = (u64 *) ((u32) pml4 + PAGE_SIZE);
    u64 *pd = (u64 *) ((u32) pdp + PAGE_SIZE);

    memset((void *) pml4, 0, PAGE_SIZE);
    memset((void *) pdp, 0, PAGE_SIZE);
    memset((void *) pd, 0, PAGE_SIZE);

    pml4[0] = ((u32) pdp) | PAGE_PRESENT | PAGE_WRITABLE;
    pml4[511] = ((u32) pdp) | PAGE_PRESENT | PAGE_WRITABLE;

    pdp[0] = ((u32) pd) | PAGE_PRESENT | PAGE_WRITABLE;
    pdp[1] = ((u32) pd + PAGE_SIZE) | PAGE_PRESENT | PAGE_WRITABLE;

    pdp[510] = ((u32) pd) | PAGE_PRESENT | PAGE_WRITABLE;
    pdp[511] = ((u32) pd + PAGE_SIZE) | PAGE_PRESENT | PAGE_WRITABLE;

    u64 addr = 0;
    for(int i = 0; i < 1024; i++) {
        pd[i] = addr | PAGE_PRESENT | PAGE_WRITABLE | PAGE_SIZE_EXTENDED;
        addr += 0x200000; // 2 MB
    }

    kiwi_boot_info.lowest_free_address = (u32) pd + (2 * PAGE_SIZE);

    long_mode((u32) &kiwi_boot_info, (u32) pml4, (u64) entry);
    return -1; // not reachable really
}
