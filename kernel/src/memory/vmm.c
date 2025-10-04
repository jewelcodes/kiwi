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

#include <kiwi/pmm.h>
#include <kiwi/vmm.h>
#include <kiwi/boot.h>
#include <kiwi/debug.h>
#include <kiwi/arch/atomic.h>
#include <kiwi/arch/paging.h>
#include <kiwi/arch/memmap.h>
#include <string.h>

VASpace vmm;

static inline int find_free_bit(u64 bitmap) {
    for(int i = 0; i < VMM_NODES_PER_PAGE; i++) {
        if(!(bitmap & (1ULL << i))) {
            return i;
        }
    }

    return -1;
}

static VMMTreeNode *vmm_allocate_node(VASpace *vas) {
    arch_switch_page_tables(vas->arch_page_tables);

    if(!vas->tree_size_pages) {
        vas->tree_size_pages = 1;
        u64 physical = pmm_alloc_page();
        if(!physical) {
            return NULL;
        }

        if(!arch_map_page(vas->arch_page_tables, (uptr) ARCH_VMM_BASE,
            physical, VMM_PROT_READ | VMM_PROT_WRITE)) {
            pmm_free_page(physical);
            return NULL;
        }

        memset((void *) ARCH_VMM_BASE, 0, PAGE_SIZE);
    }

    for(u64 i = 0; i < vas->tree_size_pages; i++) {
        u64 *usage_bitmap = (u64 *) ((uptr) ARCH_VMM_BASE + i * PAGE_SIZE);
        int free_bit = find_free_bit(*usage_bitmap);
        if(free_bit >= 0) {
            *usage_bitmap |= (1 << free_bit);
            VMMTreeNode *node = (VMMTreeNode *) ((uptr) ARCH_VMM_BASE +
                (i * PAGE_SIZE) + sizeof(u64) + (free_bit * sizeof(VMMTreeNode)));
            memset(node, 0, sizeof(VMMTreeNode));
            return node;
        }
    }

    u64 physical = pmm_alloc_page();
    if(!physical) {
        return NULL;
    }

    if(!arch_map_page(vas->arch_page_tables, (uptr) ARCH_VMM_BASE + vas->tree_size_pages * PAGE_SIZE,
        physical, VMM_PROT_READ | VMM_PROT_WRITE)) {
        pmm_free_page(physical);
        return NULL;
    }

    memset((void *) (ARCH_VMM_BASE + vas->tree_size_pages * PAGE_SIZE), 0, PAGE_SIZE);
    vas->tree_size_pages++;
    VMMTreeNode *node = (VMMTreeNode *) ((uptr) ARCH_VMM_BASE +
        ((vas->tree_size_pages - 1) * PAGE_SIZE) + sizeof(u64));
    u64 *usage_bitmap = (u64 *) ((uptr) ARCH_VMM_BASE +
        (vas->tree_size_pages - 1) * PAGE_SIZE);
    *usage_bitmap = 1;
    return node;
}

static void vmm_debug_node(VMMTreeNode *node, int recursive) {
    debug_info("node 0x%llX: va=0x%llX, pages=%llu",
        (uptr) node, node->base, node->page_count);
    debug_info("   prot=0x%X, type=0x%X, flags=0x%X, children-count=%u",
        node->prot, node->type, node->flags, node->children_count);
    debug_info("   max_va=0x%llX, max_gap=%llu",
        node->max_virtual_address, node->max_gap_page_count);

    if(!recursive) return;

    for(u16 i = 0; i < node->children_count; i++) {
        vmm_debug_node(node->children[i], 1);
    }
}

VMMTreeNode *vmm_search(VMMTreeNode *root, u64 virtual) {
    if(!root) return NULL;
    if((virtual < root->base) || (virtual >= root->max_virtual_address)) {
        return NULL;
    }

    if(!root->children_count) {
        return root;
    }

    for(u16 i = 0; i < root->children_count; i++) {
        VMMTreeNode *child = root->children[i];
        if((virtual >= child->base) && (virtual < child->max_virtual_address)) {
            VMMTreeNode *res = vmm_search(child, virtual);
            if(res) return res;
            return child;
        }
    }

    return NULL;
}

VMMTreeNode *vmm_lenient_search(VMMTreeNode *root, u64 virtual) {
    if(!root) return NULL;

    if((virtual < root->base) || (virtual >= root->max_virtual_address)) {
        return NULL;
    }

    for(u16 i = 0; i < root->children_count; i++) {
        VMMTreeNode *child = root->children[i];
        if((virtual >= child->base) && (virtual < child->max_virtual_address)) {
            VMMTreeNode *res = vmm_lenient_search(child, virtual);
            if(res) return res;
            return child;
        }
    }

    return root;
}

void vmm_init(void) {
    memset(&vmm, 0, sizeof(VASpace));
    vmm.lock = LOCK_INITIAL;

    vmm.arch_page_tables = arch_paging_init();
    debug_info("kernel page tables = 0x%llX", vmm.arch_page_tables);

    vmm.root = vmm_allocate_node(&vmm);
    if(!vmm.root) {
        debug_panic("failed to create VMM root node");
    }

    VMMTreeNode *hhdm_node = vmm_allocate_node(&vmm);
    if(!hhdm_node) {
        debug_panic("failed to create VMM HHDM node");
    }

    VMMTreeNode *kernel_node = vmm_allocate_node(&vmm);
    if(!kernel_node) {
        debug_panic("failed to create VMM kernel node");
    }

    hhdm_node->base = ARCH_HHDM_BASE;
    hhdm_node->page_count = (pmm.highest_address + LARGE_PAGE_SIZE - 1) / PAGE_SIZE;
    hhdm_node->prot = VMM_PROT_READ | VMM_PROT_WRITE;
    hhdm_node->type = VMM_TYPE_ANONYMOUS;
    hhdm_node->children_count = 0;
    hhdm_node->max_virtual_address = hhdm_node->base + pmm.highest_address;
    hhdm_node->max_gap_page_count = 0;
    hhdm_node->parent = vmm.root;
    vmm.root->children[vmm.root->children_count++] = hhdm_node;

    kernel_node->base = ARCH_KERNEL_IMAGE_BASE;
    kernel_node->page_count = (kiwi_boot_info.lowest_free_address + LARGE_PAGE_SIZE - 1)
        / PAGE_SIZE;
    kernel_node->prot = VMM_PROT_READ | VMM_PROT_WRITE | VMM_PROT_EXEC;
    kernel_node->type = VMM_TYPE_ANONYMOUS;
    kernel_node->children_count = 0;
    kernel_node->max_virtual_address = kernel_node->base
        + kiwi_boot_info.lowest_free_address - 0x100000;
    kernel_node->max_gap_page_count = 0;
    kernel_node->parent = vmm.root;
    vmm.root->children[vmm.root->children_count++] = kernel_node;

    vmm.root->base = hhdm_node->base;
    vmm.root->max_virtual_address = kernel_node->max_virtual_address;
    vmm.root->page_count = (vmm.root->max_virtual_address - vmm.root->base) / PAGE_SIZE;
    vmm.root->prot = 0;
    vmm.root->type = 0;
    vmm.root->parent = NULL;

    u64 gap = (kernel_node->base - (hhdm_node->max_virtual_address)) / PAGE_SIZE;
    vmm.root->max_gap_page_count = gap;
}