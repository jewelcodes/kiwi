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

#define EXT2_ROOT_INODE                 2
#define EXT2_SUPERBLOCK_OFFSET          1024
#define EXT2_MAGIC                      0xEF53

typedef struct Ext2Superblock {
    u32 total_inodes;
    u32 total_blocks;
    u32 reserved_blocks;
    u32 free_blocks;
    u32 free_inodes;
    u32 first_data_block;
    u32 log_block_size;
    u32 log_fragment_size;
    u32 blocks_per_group;
    u32 fragments_per_group;
    u32 inodes_per_group;
    u32 mount_time;
    u32 write_time;
    u16 mount_count;
    u16 max_mount_count;
    u16 magic;
    u16 state;
    u16 errors;
    u16 minor_revision;
    u32 check_time;
    u32 check_interval;
    u32 creator_os_id;
    u32 major_version;
    u16 reserved_user_id;
    u16 reserved_group_id;

    u32 first_inode;
    u16 inode_size;
    u16 block_group_number;
    u32 optional_features;
    u32 required_features;
    u32 read_only_features;
    u8 id[16];
    s8 volume_name[16];
    s8 last_mount_path[64];
    u32 compression_algorithm;
    u8 preallocated_blocks;
    u8 preallocated_dir_blocks;
    u16 reserved;
    s8 journal_id[16];
    u32 journal_inode;
    u32 journal_device;
    u32 orphan_inode_list;
} __attribute__((packed)) Ext2Superblock;

typedef struct Ext2BlockGroupDescriptor {
    u32 block_bitmap;
    u32 inode_bitmap;
    u32 inode_table;
    u16 free_blocks_count;
    u16 free_inodes_count;
    u16 used_dirs_count;
    u16 pad;
    u8 reserved[12];
} __attribute__((packed)) Ext2BlockGroupDescriptor;

typedef struct Ext2Inode {
    u16 mode;
    u16 user_id;
    u32 size_low;
    u32 access_time;
    u32 creation_time;
    u32 modification_time;
    u32 deletion_time;
    u16 group_id;
    u16 hard_link_count;
    u32 disk_sectors;
    u32 flags;
    u32 os_specific1;
    u32 direct_pointers[12];
    u32 singly_indirect;
    u32 doubly_indirect;
    u32 triply_indirect;
    u32 generation;
    u32 size_high;
    u32 fragment_address;
    u8 os_specific2[12];
} __attribute__((packed)) Ext2Inode;

typedef struct Ext2DirEntry {
    u32 inode;
    u16 record_length;
    u8 name_length;
    u8 file_type;
    char name[];
} __attribute__((packed)) Ext2DirEntry;

int is_ext2(Drive *drive, int partition);
usize ext2_load_file(Drive *drive, int partition, const char *path, void *buffer, usize size);
