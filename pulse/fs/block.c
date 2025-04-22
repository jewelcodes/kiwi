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
#include <stdio.h>

int read_block(FILE *disk, u64 block, u16 block_size, usize count, void *buffer) {
    if(!disk || !buffer) return 1;

    if(fseek(disk, block * block_size, SEEK_SET) != 0) {
        perror("fseek");
        return -1;
    }

    if(fread(buffer, block_size, count, disk) != count) {
        perror("fread");
        return -1;
    }

    return 0;
}

int write_block(FILE *disk, u64 block, u16 block_size, usize count, const void *buffer) {
    if(!disk || !buffer) return 1;

    if(fseek(disk, block * block_size, SEEK_SET) != 0) {
        perror("fseek");
        return -1;
    }

    if(fwrite(buffer, block_size, count, disk) != count) {
        perror("fwrite");
        return -1;
    }

    return 0;
}

int read_bit(u8 *bitmap, u64 bit) {
    return (bitmap[bit / 8] >> (bit % 8)) & 1;
}

int write_bit(u8 *bitmap, u64 bit, int value) {
    if(value) bitmap[bit / 8] |= (1 << (bit % 8));
    else bitmap[bit / 8] &= ~(1 << (bit % 8));
    return 0;
}

u64 find_lowest_free_bit(u8 *bitmap, u64 size_bits) {
    u8 byte;
    for(u64 i = 0; i < size_bits; i++) {
        if(i % 8 == 0) byte = bitmap[i / 8];
        if(!(byte & (1 << (i % 8))))
            return i;
    }
    return -1;
}

int block_status(u64 block) {
    if(!mountpoint || !mountpoint->superblock || !mountpoint->bitmap_block)
        return -1;
    if(block >= mountpoint->superblock->volume_size) return -1;

    u64 bit_offset = block + mountpoint->layer_starts[0];
    u64 bitmap_block = (bit_offset / 8 / mountpoint->block_size) + mountpoint->superblock->bitmap_block;
    u64 bit_offset_in_block = bit_offset % (mountpoint->block_size * 8);

    if(read_block(mountpoint->disk, bitmap_block,
        mountpoint->block_size, 1, mountpoint->bitmap_block)) {
        return -1;
    }

    return read_bit(mountpoint->bitmap_block, bit_offset_in_block);
}

u64 allocate_block() {
    if(!mountpoint || !mountpoint->superblock || !mountpoint->bitmap_block)
        return -1;

    u64 bit_offset = find_lowest_free_bit(mountpoint->bitmap_block,
        mountpoint->highest_layer_size);
    if(bit_offset == -1) return -1;

    if(mountpoint->bitmap_layers == 1) {
        write_bit(mountpoint->bitmap_block, bit_offset, 1);
        return bit_offset;
    }

    // avoids recursion so we have predictable stack usage
    for(int i = mountpoint->bitmap_layers - 2; i >= 0; i--) {
        bit_offset *= mountpoint->fanout;
        u64 bit_offset_into_bitmap = mountpoint->layer_starts[i] + bit_offset;

        u64 byte_offset = bit_offset_into_bitmap / 8;
        u64 bitmap_block = (byte_offset / mountpoint->block_size) +
            mountpoint->superblock->bitmap_block;
        
        u8 *offset_into_block = (u8 *) mountpoint->bitmap_block +
            byte_offset % mountpoint->block_size;

        if(read_block(mountpoint->disk, bitmap_block,
            mountpoint->block_size, 1, mountpoint->bitmap_block)) {
            return -1;
        }

        u64 old_bit_offset = bit_offset;
        bit_offset = find_lowest_free_bit(offset_into_block, mountpoint->fanout);
        if(bit_offset == -1) return -1;

        if(!i) {
            write_bit(offset_into_block, bit_offset, 1);
            if(write_block(mountpoint->disk, bitmap_block,
                mountpoint->block_size, 1, mountpoint->bitmap_block)) {
                return -1;
            }
            return bit_offset + old_bit_offset;
        }
    }

    return -1;
}