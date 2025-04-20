/*
 * pulse - a highly scalable SSD-first file system with predictable logarithmic
 * bounds across all operations
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

#include <axon/types.h>
#include <stdio.h>

/* these are tunable at format time */
#define DEFAULT_BLOCK_SIZE              4096    /* 3-bit value, valid range is powers of 2 from 4 to 512 KB */
#define DEFAULT_FANOUT_FACTOR           16      /* 2-bit, valid range is powers of 2 from 8 to 64 */
#define DEFAULT_BITMAP_LIMIT            16384   /* 2-bit, valid range is powers of 2 from 4K to 32K */

/* this is hard-coded */
#define SUPERBLOCK_BLOCK_NUMBER         64      /* superblock is always at block 64 */

/* superblock magic number */
#define SUPER_MAGIC_STRING              "pulseio"   /* first 7 bytes of the magic number */
#define SUPER_MAGIC_VERSION             0x01        /* 8th byte of the magic number */

/* revision */
#define SUPER_MAJOR_REVISION            0x0001      /* v1.0.0 */
#define SUPER_MINOR_REVISION            0x0000
#define SUPER_PATCH_REVISION            0x0000

/* superblock tuning field */
#define SUPER_TUNING_BLOCK_SIZE_MASK    0x0007
#define SUPER_TUNING_BLOCK_SIZE_4K      0x0000
#define SUPER_TUNING_BLOCK_SIZE_8K      0x0001
#define SUPER_TUNING_BLOCK_SIZE_16K     0x0002
#define SUPER_TUNING_BLOCK_SIZE_32K     0x0003
#define SUPER_TUNING_BLOCK_SIZE_64K     0x0004
#define SUPER_TUNING_BLOCK_SIZE_128K    0x0005
#define SUPER_TUNING_BLOCK_SIZE_256K    0x0006
#define SUPER_TUNING_BLOCK_SIZE_512K    0x0007

#define SUPER_TUNING_FANOUT_FACTOR_MASK 0x0018
#define SUPER_TUNING_FANOUT_FACTOR_8    0x0000
#define SUPER_TUNING_FANOUT_FACTOR_16   0x0008
#define SUPER_TUNING_FANOUT_FACTOR_32   0x0010
#define SUPER_TUNING_FANOUT_FACTOR_64   0x0018

#define SUPER_TUNING_JOURNAL_MASK       0x0060
#define SUPER_TUNING_JOURNAL_NONE       0x0000
#define SUPER_TUNING_JOURNAL_METADATA   0x0020
#define SUPER_TUNING_JOURNAL_ORDERED    0x0040

#define SUPER_TUNING_ENDIAN_MASK        0x0080
#define SUPER_TUNING_ENDIAN_LITTLE      0x0000
#define SUPER_TUNING_ENDIAN_BIG         0x0080
#define SUPER_TUNING_ENDIAN_NATIVE      __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ ? 0x0000 : 0x0080

#define SUPER_TUNING_BITMAP_LIMIT_MASK  0x0300
#define SUPER_TUNING_BITMAP_LIMIT_4096  0x0000
#define SUPER_TUNING_BITMAP_LIMIT_8192  0x0100
#define SUPER_TUNING_BITMAP_LIMIT_16384 0x0200
#define SUPER_TUNING_BITMAP_LIMIT_32768 0x0300

/* directory thresholds */
#define DIR_HASH_DEFAULT_SIZE           4       /* directories start with 4 nests */
#define DIR_HASH_GROW_LOAD_FACTOR       75      /* grow at >=75% load factor */
#define DIR_HASH_GROW_COLLISION_RATE    25      /* grow at >=25% collision rate */
#define DIR_HASH_SHRINK_LOAD_FACTOR     25      /* shrink at <25% load factor */
#define DIR_HASH_SHRINK_COLLISION_RATE  10      /* AND <10% collision rate for that load factor */
#define DIR_MAX_FILE_NAME               1006    /* 1006 bytes INCLUDING null terminator */

typedef struct SuperBlock {
    u64 magic;
    u16 major_revision;
    u16 minor_revision;
    u16 patch;
    u16 reserved1;
    u64 checksum;

    u16 superblock_size;    // length of this structure
    u16 tuning;             // contains customizability flags
    u8 status;              // clean, safe, etc
    u8 reserved2[3];

    u64 uuid[2];            // 128-bit UUID

    u64 volume_size;        // in blocks
    u64 root_inode;         // inode number of the root directory
    u64 bitmap_block;       // block number of the hierarchical bitmap
    u64 formatting_utility; // in the future I will allocate values for this
    u64 formatting_time;    // Unix time, nanosecond precision
    u64 last_mount_time;    // Unix time, nanosecond precision
    u64 last_write_time;    // Unix time, nanosecond precision
    u64 last_check_time;    // Unix time, nanosecond precision
    u64 total_mounts;       // total number of mounts with write enabled

    u32 check_interval;     // seconds, zero to disable auto-check
    u32 reserved3;

    s8 label[256];          // UTF-8, null-terminated
}__attribute__((packed)) SuperBlock;

typedef struct ExtentHeader {   /* header of every node in the B+ tree */
    u64 size;               // number of valid extents/extent branches
    u64 prev_leaf;          // block number of the previous leaf, zero in internal nodes and the first leaf
    u64 next_leaf;          // block number of the next leaf, zero in internal nodes and the last leaf
    u64 largest_offset;     // largest offset in the node - this is valid for both internal and leaf nodes
}__attribute__((packed)) ExtentHeader;

typedef struct ExtentNode {     /* node in the B+ tree */
    u64 offset;
    u64 block;          // block number of the data block (leaf) or child (internal)
    u64 count;          // contiguous block count (leaf), total block count (internal)
    u64 modified_time;  // Unix time, nanosecond precision
}__attribute__((packed)) ExtentNode;

typedef struct Inode {
    u64 number; // 0
    u32 mode; // 8
    u32 uid; // 12
    u32 gid; // 16
    u32 link_count; // 20

    u64 created_time;       // Unix time, nanosecond precision
    u64 modified_time;      // Unix time, nanosecond precision
    u64 accessed_time;      // Unix time, nanosecond precision
    u64 changed_time;       // Unix time, nanosecond precision

    u64 size;               // in bytes
    u64 extent_count;       // total count of extents - for fragmentation detection
    u64 extent_tree_root;   // block number of the root of the B+ tree
    u32 inline_size;
    u32 reserved1;
    u64 reserved2[3];       // reserved for future use

    // timestamped inode access history
    // cache of the last 8 accessed inodes, the top 3 are always there because
    // they have the most frequent access
    // the remaining 5 are a circular buffer that is updated with every access
    // if any of their access count reaches one of the top 3, their positions
    // are swapped so it doesn't get removed simply because a bunch of other
    // directories were accessed - "smart" caching
    // if it reaches the top 3, it is cached in its parent inode ONLY if it has
    // a smaller access count than the parent's lowest, it is pushed to the
    // bottom of the parent's cache and removed from the child's cache
    struct InodeHistory {
        u64 hash;           // hash of the full path to the child inode
        u64 inode;          // child inode number
        u64 access_count;   // number of accesses to the child inode
        u64 accessed_time;  // Unix time, nanosecond precision
    } __attribute__((packed)) cache[8];

    u8 payload[];           // variable length, if inline_size > 0 and bit 31 is
                            // zero in it, this is inline data; if bit 31 is one
                            // this is up to remainder of the block size filled
                            // with extent nodes
                            // if inline_size == 0, then this field is not valid
                            // and may contain garbage
}__attribute__((packed)) Inode;


typedef struct Directory {
    u64 hashmap_size;
    u64 file_count;
    u64 collision_count;
    u64 last_resize_time;
    u64 last_expand_time;
    u64 last_shrink_time;
    u64 total_resizes;
    u64 total_expands;
    u64 total_shrinks;

    u64 hashmap[];      // block numbers of the nests
}__attribute__((packed)) Directory;


int h = sizeof(Directory);

typedef struct DirectoryEntry {
    u64 inode;
    u64 reserved;
    s8 name[DIR_MAX_FILE_NAME];     // UTF-8, null-terminated
}__attribute__((packed)) DirectoryEntry;

typedef struct DirectoryHashNest {
    u64 next;
    DirectoryEntry file[];
}__attribute__((packed)) DirectoryHashNest;

int format(const char *path, usize size, usize block_size, usize fanout);
int read_block(FILE *disk, u64 block, u16 block_size, usize count, void *buffer);
int write_block(FILE *disk, u64 block, u16 block_size, usize count, const void *buffer);
