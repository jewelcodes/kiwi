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

#include <pulse/pulse.h>
#include <pulse/cli.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

int format(const char *path, usize size, usize block_size, usize fanout) {
    usize block_count = size / block_size;

    void *data = malloc(block_size);
    if(!data) return -1;

    memset(data, 0, block_size);

    FILE *disk = fopen(path, "wb+");
    if(!disk) {
        free(data);
        return 1;
    }

    for(usize i = 0; i < block_count; i++) {
        if(fwrite(data, block_size, 1, disk) != 1) {
            fclose(disk);
            free(data);
            return 1;
        }
    }

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    u64 time_ns = ts.tv_sec * 1000000000ULL + ts.tv_nsec;

    SuperBlock *superblock = (SuperBlock *)data;
    memcpy(&superblock->magic, SUPER_MAGIC_STRING, 8);
    superblock->major_revision = SUPER_MAJOR_REVISION;
    superblock->minor_revision = SUPER_MINOR_REVISION;
    superblock->patch = SUPER_PATCH_REVISION;
    superblock->superblock_size = sizeof(SuperBlock);

    superblock->tuning = SUPER_TUNING_ENDIAN_NATIVE;
    superblock->tuning |= SUPER_TUNING_JOURNAL_NONE; // TODO

    // switch case and not bit arithmetic so we can validate the config here
    switch(block_size) {
    case 4096:
        superblock->tuning |= SUPER_TUNING_BLOCK_SIZE_4K;
        break;
    case 8192:
        superblock->tuning |= SUPER_TUNING_BLOCK_SIZE_8K;
        break;
    case 16384:
        superblock->tuning |= SUPER_TUNING_BLOCK_SIZE_16K;
        break;
    case 32768:
        superblock->tuning |= SUPER_TUNING_BLOCK_SIZE_32K;
        break;
    case 65536:
        superblock->tuning |= SUPER_TUNING_BLOCK_SIZE_64K;
        break;
    case 131072:
        superblock->tuning |= SUPER_TUNING_BLOCK_SIZE_128K;
        break;
    case 262144:
        superblock->tuning |= SUPER_TUNING_BLOCK_SIZE_256K;
        break;
    case 524288:
        superblock->tuning |= SUPER_TUNING_BLOCK_SIZE_512K;
        break;
    default:
        return 1;
    }

    switch(fanout) {
    case 8:
        superblock->tuning |= SUPER_TUNING_FANOUT_FACTOR_8;
        break;
    case 16:
        superblock->tuning |= SUPER_TUNING_FANOUT_FACTOR_16;
        break;
    case 32:
        superblock->tuning |= SUPER_TUNING_FANOUT_FACTOR_32;
        break;
    case 64:
        superblock->tuning |= SUPER_TUNING_FANOUT_FACTOR_64;
        break;
    default:
        return 1;
    }

    usize bitmap_limit = DEFAULT_BITMAP_LIMIT;
    switch(bitmap_limit) {
    case 4096:
        superblock->tuning |= SUPER_TUNING_BITMAP_LIMIT_4096;
        break;
    case 8192:
        superblock->tuning |= SUPER_TUNING_BITMAP_LIMIT_8192;
        break;
    case 16384:
        superblock->tuning |= SUPER_TUNING_BITMAP_LIMIT_16384;
        break;
    case 32768:
        superblock->tuning |= SUPER_TUNING_BITMAP_LIMIT_32768;
        break;
    default:
        return 1;
    }

    // TODO: proper UUID generation
    superblock->uuid[0] = 0x1234567890abcdefULL;
    superblock->uuid[1] = 0xfedcba0987654321ULL;

    superblock->volume_size = block_count;
    superblock->bitmap_block = SUPERBLOCK_BLOCK_NUMBER + 1;
    superblock->formatting_utility = 1;
    superblock->formatting_time = time_ns;

    // calculate the size and structure of the hierarchical bitmap so we know
    // how many blocks to allocate for it
    usize bitmap_size_bits = block_count;
    usize highest_layer = bitmap_size_bits;
    u8 layer_count = 1;

    while(highest_layer > DEFAULT_BITMAP_LIMIT) {
        highest_layer /= fanout;
        bitmap_size_bits += highest_layer;
        layer_count++;
    }

    u64 bitmap_blocks = (((bitmap_size_bits + 7) / 8) + block_size - 1) / block_size;
    u64 root_inode = SUPERBLOCK_BLOCK_NUMBER + 1 + bitmap_blocks;
    superblock->root_inode = root_inode;

    if(write_block(disk, SUPERBLOCK_BLOCK_NUMBER, block_size, 1, superblock))
        return 1;

    // now we need to update the status of all the blocks up to the root inode
    // to be allocated - on the lowest level of the bitmap, this is simply setting
    // the corresponding bit to 1
    // for higher levels on the bitmap, the bit is only set to 1 if all its children
    // are set to 1
    // we also need to track where each layer starts and ends because they are
    // contiguous and not block-aligned nor are they the same size
    // the first layer pointed to by the superblock is the topmost (smallest) layer
    // and the last layer is the bottommost (largest) layer

    printf("    🛠️  building %u layer%s of hierarchical bitmap with fanout factor %zu\n",
        layer_count, layer_count > 1 ? "s" : "", fanout);

    u64 *layer_sizes = calloc(layer_count, sizeof(u64)); // size in bits
    u64 *layer_starts = calloc(layer_count, sizeof(u64));   // starting bit offset
    if(!layer_starts) {
        fclose(disk);
        free(data);
        return 1;
    }

    for(int i = layer_count-1; i >= 0; i--) {
        if(i == layer_count-1) {
            layer_starts[i] = 0;
            layer_sizes[i] = highest_layer;
        } else  {
            if(i == layer_count-2) layer_starts[i] = highest_layer;
            else layer_starts[i] = layer_starts[i+1] * fanout;

            layer_sizes[i] = layer_sizes[i+1] * fanout;
        }
    }

    u64 *layer_mapping_size = calloc(layer_count, sizeof(u64));
    if(!layer_mapping_size) {
        fclose(disk);
        free(data);
        free(layer_starts);
        return 1;
    }

    for(int i = 0; i < layer_count; i++) {
        if(i == 0) layer_mapping_size[i] = 1;
        else layer_mapping_size[i] = layer_mapping_size[i-1] * fanout;
    }

    for(int i = 0; i < layer_count; i++) {
        printf("    🛠️  layer %d%s: bits %llu -> %llu (%d bits, each maps ", i,
            !i ? " (bottom)" : (i == layer_count-1) ? " (top)" : "",
            layer_starts[i],
            (i > 0) ? layer_starts[i-1]-1 : layer_starts[i]+block_count-1,
            (int)layer_sizes[i]);

        u64 size = layer_mapping_size[i] * block_size;
        if(size >> 40)
            printf("%llu TB", size >> 40);
        else if(size >> 30)
            printf("%llu GB", size >> 30);
        else if(size >> 20)
            printf("%llu MB", size >> 20);
        else if(size >> 10)
            printf("%llu KB", size >> 10);
        else
            printf("%llu B", size);
        printf(")\n");
    }

    // now check how many total blocks we just allocated, including
    // preallocating the root inode block
    u64 allocated_blocks = root_inode + 1;
    u8 *bitmap = calloc(bitmap_blocks, block_size);
    if(!bitmap) {
        fclose(disk);
        free(data);
        free(layer_mapping_size);
        free(layer_starts);
        return 1;
    }

    // build the hierarchy
    for(int i = 0; i < allocated_blocks; i++) {
        write_bit(bitmap, layer_starts[0] + i, 1);

        for(int j = 1; j < layer_count; j++) {
            u64 sum = 0;
            u64 factor = 1;
            for(int k = 0; k < j; k++) {
                factor *= fanout;
            }

            u64 bit = (i / factor);
            u64 start = bit * fanout;

            for(int k = 0; k < fanout; k++) {
                sum += read_bit(bitmap, layer_starts[j-1] + start + k);
            }

            if(sum == fanout) {
                write_bit(bitmap, layer_starts[j] + bit, 1);
            }
        }
    }

    printf("    🛠️  writing %llu blocks of bitmap data\n", bitmap_blocks);
    if(write_block(disk, superblock->bitmap_block, block_size, bitmap_blocks, bitmap)) {
        fclose(disk);
        free(data);
        free(bitmap);
        free(layer_mapping_size);
        free(layer_starts);
        return 1;
    }

    free(bitmap);
    free(layer_mapping_size);
    free(layer_starts);

    // now we need to write the root inode
    Inode *inode = (Inode *)data;
    memset(inode, 0, block_size);

    inode->number = 1;
    inode->mode = INODE_MODE_TYPE_DIR | INODE_MODE_U_RWX | INODE_MODE_G_R;
    inode->mode |= INODE_MODE_G_X | INODE_MODE_O_R | INODE_MODE_O_X;
    inode->uid = 0; // root
    inode->gid = 0;
    inode->link_count = 1;
    inode->size = 0;

    inode->created_time = time_ns;
    inode->modified_time = time_ns;
    inode->accessed_time = time_ns;
    inode->changed_time = time_ns;

    // explicitly setting these to zero because the hashmap need to be allocated
    // on write
    inode->size = 0;
    inode->extent_count = 0;
    inode->extent_tree_root = 0;
    inode->inline_size = 0;

    if(write_block(disk, root_inode, block_size, 1, inode)) {
        fclose(disk);
        free(data);
        return 1;
    }

    printf("    🛠️  created root directory at inode %llu\n", root_inode);

    u64 overhead = allocated_blocks * block_size;

    printf("    ✅ formatted disk image %s with size %zu %s, overhead space %llu %s (%.2f%%)\n",
        path,
        size >> 40 ? size >> 40 : size >> 30 ? size >> 30 : size >> 20 ? size >> 20 : size >> 10 ? size >> 10 : size,
        size >> 40 ? "TB" : size >> 30 ? "GB" : size >> 20 ? "MB" : size >> 10 ? "KB" : "B",
        overhead >> 40 ? overhead >> 40 : overhead >> 30 ? overhead >> 30 : overhead >> 20 ? overhead >> 20 : overhead >> 10 ? overhead >> 10 : overhead,
        overhead >> 40 ? "TB" : overhead >> 30 ? "GB" : overhead >> 20 ? "MB" : overhead >> 10 ? "KB" : "B",
        ((float)overhead * 100 / size));
    fclose(disk);
    free(data);

    return 0;
}