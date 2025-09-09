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

int read_inode(u64 inode, Inode *buffer) {
    if(!mountpoint || !mountpoint->superblock || !inode || !buffer)
        return -1;

    if(read_block(mountpoint->disk, inode, mountpoint->block_size, 1, mountpoint->metadata_block))
        return -1;
    
    Inode *temp_inode = (Inode *) mountpoint->metadata_block;
    memcpy(buffer, temp_inode, temp_inode->inline_size + sizeof(Inode));
    return 0;
}

int write_inode(u64 inode, const Inode *buffer) {
    if(!mountpoint || !mountpoint->superblock || !inode || !buffer)
        return -1;

    memcpy(mountpoint->metadata_block, buffer, buffer->inline_size + sizeof(Inode));
    if(write_block(mountpoint->disk, inode, mountpoint->block_size, 1, mountpoint->metadata_block))
        return -1;

    return 0;
}

int dump_inode(u64 inode) {
    Inode *buf = (Inode *) mountpoint->metadata_block;
    if(read_inode(inode, buf))
        return -1;
    
    printf(ESC_CYAN ESC_BOLD "Inode %llu\n" ESC_RESET, inode);
    printf("  Mode: 0x%04X (%c%c%c%c%c%c%c%c%c%c)\n", buf->mode,
        (buf->mode & INODE_MODE_TYPE_DIR) ? 'd' :
        (buf->mode & INODE_MODE_TYPE_LNK) ? 'l' : '-',
        (buf->mode & INODE_MODE_U_R)  ? 'r' : '-', // owner read
        (buf->mode & INODE_MODE_U_W)  ? 'w' : '-', // owner write
        (buf->mode & INODE_MODE_U_X)  ? 'x' : '-', // owner execute
        (buf->mode & INODE_MODE_G_R)  ? 'r' : '-', // group read
        (buf->mode & INODE_MODE_G_W)  ? 'w' : '-', // group write
        (buf->mode & INODE_MODE_G_X)  ? 'x' : '-', // group execute
        (buf->mode & INODE_MODE_O_R)  ? 'r' : '-', // others read
        (buf->mode & INODE_MODE_O_W)  ? 'w' : '-', // others write
        (buf->mode & INODE_MODE_O_X)  ? 'x' : '-'  // others execute
    );

    printf("  UID: %u\n", buf->uid);
    printf("  GID: %u\n", buf->gid);
    printf("  Link count: %u\n", buf->link_count);
    printf("  Created time: %llu\n", buf->created_time);
    printf("  Modified time: %llu\n", buf->modified_time);
    printf("  Accessed time: %llu\n", buf->accessed_time);
    printf("  Changed time: %llu\n", buf->changed_time);
    printf("  Size: %llu bytes\n", buf->size);
    printf("  Inline size: %u bytes\n", buf->inline_size);
    printf("  Extent count: %llu\n", buf->extent_count);
    printf("  Extent tree root: %llu\n", buf->extent_tree_root);

    if(buf->inline_size > 0) {
        printf("  Inline data (first 64 bytes or up to inline size):\n    ");
        for(u32 i = 0; i < buf->inline_size && i < 64; i++) {
            printf("%02X ", buf->payload[i]);
        }
        printf("\n");
    }

    return 0;
}
