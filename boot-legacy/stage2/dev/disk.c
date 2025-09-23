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

#include <boot/bios.h>
#include <boot/disk.h>
#include <string.h>
#include <stdio.h>

static Registers disk_regs;
static u8 disk_buffer[4096];

Drive boot_drive;
Drive drives[MAX_DRIVES];
int drive_count = 0;

int disk_init(void) {
    boot_drive.drive_number = bios_boot_info.boot_disk;
    memcpy(&boot_drive.mbr_partitions[0],&bios_boot_info.boot_partition,
        sizeof(MBRPartition));

    BIOSDriveInfo *info = &boot_drive.info;
    memset(info, 0, sizeof(BIOSDriveInfo));
    info->buffer_size = sizeof(BIOSDriveInfo);

    disk_regs.eax = BIOS_DISK_GET_INFO << 8;
    disk_regs.edx = boot_drive.drive_number;
    disk_regs.ds = (((u32) info) >> 4) & 0xFFFF;
    disk_regs.esi = ((u32) info) & 0x0F;
    bios_int(0x13, &disk_regs);

    if((disk_regs.eflags & 1) || ((disk_regs.eax >> 8) & 0xFF)) {
        printf("Failed to query boot device info\n");
        for(;;);
    }

    for(int i = 0; i < MAX_DRIVES; i++) {
        info = &drives[drive_count].info;
        memset(info, 0, sizeof(BIOSDriveInfo));
        info->buffer_size = sizeof(BIOSDriveInfo);

        disk_regs.eax = BIOS_DISK_GET_INFO << 8;
        disk_regs.edx = i | 0x80;   // hdd
        disk_regs.ds = (((u32) info) >> 4) & 0xFFFF;
        disk_regs.esi = ((u32) info) & 0x0F;

        bios_int(0x13, &disk_regs);
        if((disk_regs.eflags & 1) || ((disk_regs.eax >> 8) & 0xFF)) {
            continue;
        }

        if(!info->sectors || !info->bytes_per_sector) {
            continue;
        }

        drives[drive_count].drive_number = i | 0x80;
        if(disk_read(&drives[drive_count], 0, 1, disk_buffer) < 0) {
            continue;
        }

        memcpy(&drives[drive_count].mbr_partitions[0],
            &disk_buffer[MBR_PARTITION_OFFSET], sizeof(MBRPartition) * 4);

        // TODO: GPT partitions

        drive_count++;
    }

    return drive_count;
}

int disk_read(Drive *drive, u64 lba, u16 sectors, void *buffer) {
    if(!sectors) {
        return -1;
    }

    DiskAddressPacket dap;

    for(u16 i = 0; i < sectors; i++) {
        dap.size = sizeof(DiskAddressPacket);
        dap.reserved = 0;
        dap.sectors = 1;
        dap.offset = ((u32) disk_buffer) & 0x0F;
        dap.segment = (((u32) disk_buffer) >> 4) & 0xFFFF;
        dap.lba = lba + i;

        disk_regs.eax = BIOS_DISK_READ << 8;
        disk_regs.edx = drive->drive_number;
        disk_regs.ds = (((u32) &dap) >> 4) & 0xFFFF;
        disk_regs.esi = ((u32) &dap) & 0x0F;
        bios_int(0x13, &disk_regs);

        if(disk_regs.eflags & 1) {
            return -1;
        }

        memcpy((u8 *) buffer + (i * drive->info.bytes_per_sector),
            disk_buffer, drive->info.bytes_per_sector);
    }

    return 0;
}
