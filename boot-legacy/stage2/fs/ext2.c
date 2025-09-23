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

#include <boot/disk.h>
#include <fs/ext2.h>
#include <stdio.h>

static u8 superblock_buffer[4096];

int is_ext2(Drive *drive, int partition) {
    if(!drive || partition < 0 || partition >= 4) {
        return 0;
    }

    u64 starting_lba = drive->mbr_partitions[partition].start_lba;
    starting_lba += EXT2_SUPERBLOCK_OFFSET / drive->info.bytes_per_sector;
    if(disk_read(drive, starting_lba,
        (sizeof(Ext2Superblock) + drive->info.bytes_per_sector - 1) /
        drive->info.bytes_per_sector, superblock_buffer) < 0) {
        return 0;
    }

    Ext2Superblock *sb = (Ext2Superblock *) superblock_buffer;
    if(sb->magic != EXT2_MAGIC) {
        return 0;
    }

    return 1;
}