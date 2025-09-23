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

#define MAX_DRIVES                  8

#define BIOS_DISK_READ              0x42
#define BIOS_DISK_GET_INFO          0x48

#define MBR_PARTITION_OFFSET        446

typedef struct BIOSDriveInfo {
    u16 buffer_size;
    u16 info;
    u32 cylinders;          // deprecated
    u32 heads;              // deprecated
    u32 sectors_per_track;  // deprecated
    u64 sectors;
    u16 bytes_per_sector;
    u32 edd_ptr;
} __attribute__((packed)) BIOSDriveInfo;

typedef struct DiskAddressPacket {
    u8 size;
    u8 reserved;
    u16 sectors;
    u16 offset;
    u16 segment;
    u64 lba;
} __attribute__((packed)) DiskAddressPacket;

typedef struct MBRPartition {
    u8 bootable;
    u8 start_chs[3];
    u8 type;
    u8 end_chs[3];
    u32 start_lba;
    u32 sectors;
} __attribute__((packed)) MBRPartition;

typedef struct Drive {
    BIOSDriveInfo info;
    u8 drive_number;
    MBRPartition mbr_partitions[4];
    // TODO: GPT partitions
} Drive;

extern Drive boot_drive;
extern Drive drives[];
extern int drive_count;

int disk_init(void);
int disk_read(Drive *drive, u64 lba, u16 sectors, void *buffer);
