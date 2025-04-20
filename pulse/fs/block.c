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

