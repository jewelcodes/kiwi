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
#include <string.h>
#include <time.h>

int write_to_inode(u64 inode, const void *buf, u64 offset, u64 size) {
    if(!mountpoint || !mountpoint->superblock || !inode || !buf || !size)
        return -1;
    
    u32 max_inline_size = mountpoint->block_size - sizeof(Inode);
    Inode *inode_buf = (Inode *) mountpoint->metadata_block;
    if(read_inode(inode, inode_buf))
        return -1;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    u64 time_ns = ts.tv_sec * 1000000000ULL + ts.tv_nsec;

    if((!inode_buf->extent_tree_root) && (offset + size <= max_inline_size)) {
        memcpy(inode_buf->payload + offset, buf, size);
        if(offset + size > inode_buf->inline_size)
            inode_buf->inline_size = offset + size;

        inode_buf->size = inode_buf->inline_size;
        inode_buf->modified_time = time_ns;
        inode_buf->changed_time = time_ns;
        return write_inode(inode, inode_buf);
    } else {
        printf("todo: write to extent\n");
        return -1;
    }

    return 0;
}
