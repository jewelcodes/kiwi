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
#include <kiwi/arch/atomic.h>

#define VMM_FANOUT                  8
#define VMM_NODES_PER_PAGE          ((PAGE_SIZE / sizeof(VMMTreeNode)) - 1)

#define VMM_PROT_READ               0x0001
#define VMM_PROT_WRITE              0x0002
#define VMM_PROT_EXEC               0x0004
#define VMM_PROT_USER               0x0008

#define VMM_TYPE_ANONYMOUS          0x01
#define VMM_TYPE_FILE_BACKED        0x02
#define VMM_TYPE_SHARED             0x03
#define VMM_TYPE_DEVICE             0x04

#define VMM_FLAGS_GUARD             0x01
#define VMM_FLAGS_COW               0x02
#define VMM_FLAGS_UNALLOCATED       0x04

typedef struct VMMTreeNode {
    u64 base;
    u64 page_count;
    u16 prot;
    u8 type;
    u8 flags;
    u16 children_count;
    u16 reserved;

    uptr backing;
    uptr file_offset;

    u64 max_virtual_address;    // largest virtual address in the subtree
    u64 max_gap_page_count;     // largest gap (in pages) in the subtree

    struct VMMTreeNode *parent;
    struct VMMTreeNode *children[VMM_FANOUT];
} VMMTreeNode;

typedef struct VASpace {
    lock_t lock;
    VMMTreeNode *root;
    uptr arch_page_tables;
    u64 tree_size_pages;
} VASpace;

void vmm_init(void);
VMMTreeNode *vmm_search(VMMTreeNode *root, u64 virtual);
VMMTreeNode *vmm_lenient_search(VMMTreeNode *root, u64 virtual);
VMMTreeNode *vmm_create_node(VASpace *vas, const VMMTreeNode *new_node);
int vmm_delete_node(VASpace *vas, VMMTreeNode *node);
