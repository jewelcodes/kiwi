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
#include <boot/fs.h>
#include <fs/ext2.h>
#include <string.h>
#include <stdio.h>

static u8 superblock_buffer[4096];
static u8 inode_block[4096];
static u8 bgdt_buffer[4096];
static char path_buffer[128];

static Ext2Superblock *read_superblock(Drive *drive, int partition) {
    if(!drive || partition < 0 || partition >= 4) {
        return NULL;
    }

    u64 starting_lba = drive->mbr_partitions[partition].start_lba;
    starting_lba += EXT2_SUPERBLOCK_OFFSET / drive->info.bytes_per_sector;
    if(disk_read(drive, starting_lba,
        (sizeof(Ext2Superblock) + drive->info.bytes_per_sector - 1) /
        drive->info.bytes_per_sector, superblock_buffer) < 0) {
        return NULL;
    }

    Ext2Superblock *superblock = (Ext2Superblock *) superblock_buffer;
    return superblock;
}

int is_ext2(Drive *drive, int partition) {
    if(!drive || partition < 0 || partition >= 4) {
        return 0;
    }

    Ext2Superblock *superblock = read_superblock(drive, partition);
    if(!superblock) {
        return 0;
    }

    return superblock->magic == EXT2_MAGIC;
}

static int read_block(Drive *drive, int partition, u32 block, void *buffer) {
    if(!drive || partition < 0 || partition >= 4) {
        return -1;
    }

    Ext2Superblock *superblock = (Ext2Superblock *) superblock_buffer;
    usize block_size = 1024 << superblock->log_block_size;
    u64 starting_lba = drive->mbr_partitions[partition].start_lba;
    starting_lba += (block * block_size) / drive->info.bytes_per_sector;

    return disk_read(drive, starting_lba + ((block * block_size)
        % drive->info.bytes_per_sector),
        (block_size + drive->info.bytes_per_sector - 1) /
        drive->info.bytes_per_sector, buffer);
}

static usize read_inode(Drive *drive, int partition, int directory,
                        u32 inode_num, void *buffer, usize size) {
    if(!drive || partition < 0 || partition >= 4) {
        return 0;
    }

    Ext2Superblock *superblock = (Ext2Superblock *) superblock_buffer;
    usize block_size = 1024 << superblock->log_block_size;
    u64 bgdt_block = block_size == 1024 ? 2 : 1;
    u16 inode_size = superblock->major_version == 0 ? 128 : superblock->inode_size;

    u32 inodes_per_group = superblock->inodes_per_group;
    u32 block_group = (inode_num - 1) / inodes_per_group;
    u32 index_within_group = (inode_num - 1) % inodes_per_group;

    bgdt_block += (block_group * sizeof(Ext2BlockGroupDescriptor)) / block_size;
    if(read_block(drive, partition, bgdt_block, bgdt_buffer) < 0) {
        return 0;
    }

    Ext2BlockGroupDescriptor *bg_desc = (Ext2BlockGroupDescriptor *) ((void *)
        bgdt_buffer + (block_group * sizeof(Ext2BlockGroupDescriptor)));

    u32 inode_table_block = bg_desc->inode_table;
    u32 block_offset = (index_within_group * inode_size) / block_size;
    u32 offset_in_block = (index_within_group * inode_size) % block_size;

    if(read_block(drive, partition, inode_table_block + block_offset, inode_block) < 0) {
        return 0;
    }

    Ext2Inode *inode = (Ext2Inode *) ((void *) inode_block + offset_in_block);

    if(directory && !EXT2_IS_DIRECTORY(inode->mode)) {
        return 0;
    }

    if(!directory && !EXT2_IS_REGULAR(inode->mode)) {
        return 0;
    }

    usize read_bytes = 0;

    for(int i = 0; i < 12; i++) {
        if(!inode->direct_pointers[i]) {
            break;
        }

        if(read_block(drive, partition, inode->direct_pointers[i],
            (u8 *) buffer + read_bytes) < 0) {
            return read_bytes;
        }

        read_bytes += block_size;
    }

    return read_bytes;
}

usize ext2_load_file(Drive *drive, int partition, const char *path, void *buffer, usize size) {
    if(!drive || partition < 0 || partition >= 4) {
        return 0;
    }

    Ext2Superblock *superblock = read_superblock(drive, partition);
    if(!superblock) {
        return 0;
    }

    usize dir_size = read_inode(drive, partition, 1, EXT2_ROOT_INODE, buffer, size);
    if(!dir_size) {
        return 0;
    }

    int components = parse_path(path, 0, NULL, 0);
    if(components <= 0) {
        return 0;
    }

    int current_component = 0;

search:
    if(parse_path(path, current_component, path_buffer, sizeof(path_buffer)) != components) {
        return 0;
    }
    
    usize offset = 0;
    while(offset < dir_size) {
        Ext2DirEntry *entry = (Ext2DirEntry *) ((u8 *) buffer + offset);
        if(!entry->record_length) {
            break;
        }

        if(!entry->inode) {
            offset += entry->record_length;
            continue;
        }

        if((entry->name_length == strlen(path_buffer))
            && !memcmp(entry->name, path_buffer, entry->name_length)) {
            if(current_component == components - 1) {
                return read_inode(drive, partition, 0, entry->inode, buffer, size);
            } else {
                // subdirectory
                dir_size = read_inode(drive, partition, 1, entry->inode, buffer, size);
                if(!dir_size) {
                    return 0;
                }

                current_component++;
                goto search;
            }
        }

        offset += entry->record_length;
    }

    return 0;
}
