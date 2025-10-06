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

VASpace kvmm;

static inline int find_free_bit(u64 bitmap) {
    for(int i = 0; i < VMM_NODES_PER_PAGE; i++) {
        if(!(bitmap & (1ULL << i))) {
            return i;
        }
    }

    return -1;
}

static VMMTreeNode *vmm_allocate_node(VASpace *vas) {
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
    memset(&kvmm, 0, sizeof(VASpace));
    kvmm.lock = LOCK_INITIAL;

    kvmm.arch_page_tables = arch_paging_init();
    debug_info("kernel page tables = 0x%llX", kvmm.arch_page_tables);

    kvmm.root = vmm_allocate_node(&kvmm);
    if(!kvmm.root) {
        debug_panic("failed to create VMM root node");
    }

    VMMTreeNode *hhdm_node = vmm_allocate_node(&kvmm);
    if(!hhdm_node) {
        debug_panic("failed to create VMM HHDM node");
    }

    VMMTreeNode *kernel_node = vmm_allocate_node(&kvmm);
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
    hhdm_node->parent = kvmm.root;
    kvmm.root->children[kvmm.root->children_count++] = hhdm_node;

    kernel_node->base = ARCH_KERNEL_IMAGE_BASE;
    kernel_node->page_count = (kiwi_boot_info.lowest_free_address + LARGE_PAGE_SIZE - 1)
        / PAGE_SIZE;
    kernel_node->prot = VMM_PROT_READ | VMM_PROT_WRITE | VMM_PROT_EXEC;
    kernel_node->type = VMM_TYPE_ANONYMOUS;
    kernel_node->children_count = 0;
    kernel_node->max_virtual_address = kernel_node->base
        + kiwi_boot_info.lowest_free_address - 0x100000;
    kernel_node->max_gap_page_count = 0;
    kernel_node->parent = kvmm.root;
    kvmm.root->children[kvmm.root->children_count++] = kernel_node;

    kvmm.root->base = hhdm_node->base;
    kvmm.root->max_virtual_address = kernel_node->max_virtual_address;
    kvmm.root->page_count = (kvmm.root->max_virtual_address - kvmm.root->base) / PAGE_SIZE;
    kvmm.root->prot = 0;
    kvmm.root->type = 0;
    kvmm.root->parent = NULL;

    u64 gap = (kernel_node->base - (hhdm_node->max_virtual_address)) / PAGE_SIZE;
    kvmm.root->max_gap_page_count = gap;
}

VMMTreeNode *vmm_create_node(VASpace *vas, const VMMTreeNode *new_node) {
    if(!vas || !new_node || !vas->root) {
        return NULL;
    }

    VMMTreeNode *parent = vmm_lenient_search(vas->root, new_node->base);
    if(!parent) {
        VMMTreeNode *new_root = vmm_allocate_node(vas);
        if(!new_root) {
            debug_error("failed to allocate new VMM root node");
            return NULL;
        }

        u64 base = (new_node->base < vas->root->base)
            ? new_node->base : vas->root->base;
        u64 end = ((new_node->base + new_node->page_count * PAGE_SIZE)
            > vas->root->max_virtual_address)
            ? (new_node->base + new_node->page_count * PAGE_SIZE)
            : vas->root->max_virtual_address;

        new_root->base = base;
        new_root->page_count = (end - base + PAGE_SIZE - 1) / PAGE_SIZE;
        new_root->prot = 0;
        new_root->type = 0;
        new_root->children_count = 1;
        new_root->children[0] = vas->root;
        new_root->max_virtual_address = end;

        u64 gap1 = (vas->root->base - new_root->base) / PAGE_SIZE;
        u64 gap2 = (new_root->max_virtual_address - vas->root->max_virtual_address)
            / PAGE_SIZE;
        new_root->max_gap_page_count = (gap1 > gap2) ? gap1 : gap2;
        vas->root->parent = new_root;
        vas->root = new_root;

        parent = new_root;
    }

    VMMTreeNode *new_vmm_node = vmm_allocate_node(vas);
    if(!new_vmm_node) {
        debug_error("failed to allocate new VMM node");
        return NULL;
    }

    memcpy(new_vmm_node, new_node, sizeof(VMMTreeNode));
    new_vmm_node->parent = parent;
    new_vmm_node->children_count = 0;
    new_vmm_node->max_virtual_address = new_vmm_node->base
        + new_vmm_node->page_count * PAGE_SIZE;
    new_vmm_node->max_gap_page_count = 0;

    if(parent->children_count >= VMM_FANOUT) {
        // find node with smallest gap to split
        u16 min_gap_index = 0;
        u64 min_gap = (parent->children[0]->base - parent->base) / PAGE_SIZE;
        for(u16 i = 1; i < parent->children_count; i++) {
            u64 gap = (parent->children[i]->base - parent->base) / PAGE_SIZE;
            if(gap < min_gap) {
                min_gap = gap;
                min_gap_index = i;
            }
        }

        VMMTreeNode *new_intermediate_node = vmm_allocate_node(vas);
        if(!new_intermediate_node) {
            debug_error("failed to allocate new intermediate VMM node");
            return NULL;
        }

        u64 base = (new_node->base < parent->children[min_gap_index]->base)
            ? new_node->base : parent->children[min_gap_index]->base;
        u64 end = ((new_node->base + new_node->page_count * PAGE_SIZE)
            > parent->children[min_gap_index]->max_virtual_address)
            ? (new_node->base + new_node->page_count * PAGE_SIZE)
            : parent->children[min_gap_index]->max_virtual_address;

        new_intermediate_node->base = base;
        new_intermediate_node->page_count = (end - base + PAGE_SIZE - 1) / PAGE_SIZE;
        new_intermediate_node->prot = 0;
        new_intermediate_node->type = 0;
        new_intermediate_node->children_count = 1;
        new_intermediate_node->children[0] = parent->children[min_gap_index];
        new_intermediate_node->max_virtual_address = end;

        u64 gap1 = (parent->children[min_gap_index]->base - new_intermediate_node->base) / PAGE_SIZE;
        u64 gap2 = (new_intermediate_node->max_virtual_address - parent->children[min_gap_index]->max_virtual_address)
            / PAGE_SIZE;
        new_intermediate_node->max_gap_page_count = (gap1 > gap2) ? gap1 : gap2;
        new_intermediate_node->parent = parent;
        parent->children[min_gap_index]->parent = new_intermediate_node;
        parent->children[min_gap_index] = new_intermediate_node;

        parent = new_intermediate_node;
    }

    parent->children[parent->children_count++] = new_vmm_node;
    return new_vmm_node;
}

void *vmm_allocate(VASpace *vas, u64 base, u64 limit, usize page_count, u16 prot) {
    if(!vas) {
        vas = &kvmm;
    }

    if(!vas->root || !page_count || base >= limit) {
        return NULL;
    }

    arch_spinlock_acquire(&vas->lock);
    arch_switch_page_tables(vas->arch_page_tables);

    VMMTreeNode *parent = vmm_lenient_search(vas->root, base);
    if(!parent) {
        goto allocate;
    }

    if(parent->children_count == 0) {
        parent = parent->parent;
    }

    u64 addr = base;
    while(addr + page_count * PAGE_SIZE <= limit) {
        int conflict = 0;
        for(u16 i = 0; i < parent->children_count; i++) {
            VMMTreeNode *child = parent->children[i];

            if(!((addr + page_count * PAGE_SIZE <= child->base)
                || (addr >= child->max_virtual_address))) {
                conflict = 1;
                addr = child->max_virtual_address;
                break;
            }
        }

        if(conflict) {
            continue;
        }

        break;
    }

    if(addr + page_count * PAGE_SIZE > limit) {
        arch_spinlock_release(&vas->lock);
        return NULL;
    }

    base = addr;

allocate:
    VMMTreeNode new_node;
    memset(&new_node, 0, sizeof(VMMTreeNode));
    new_node.base = base;
    new_node.page_count = page_count;
    new_node.prot = prot;
    new_node.type = VMM_TYPE_ANONYMOUS;
    new_node.flags = VMM_FLAGS_UNALLOCATED;

    VMMTreeNode *res = vmm_create_node(vas, &new_node);
    if(!res) {
        arch_spinlock_release(&vas->lock);
        return NULL;
    }

    arch_spinlock_release(&vas->lock);
    return (void *) res->base;
}

int vmm_page_fault(VASpace *vas, u64 virtual, int user, int write, int exec) {
    if(!vas || !vas->root) {
        return -1;
    }

    arch_spinlock_acquire(&vas->lock);
    arch_switch_page_tables(vas->arch_page_tables);

    VMMTreeNode *node = vmm_search(vas->root, virtual);
    if(!node) goto fail;

    if(user && !(node->prot & VMM_PROT_USER)) goto fail;
    if(write && !(node->prot & VMM_PROT_WRITE)) goto fail;
    if(exec && !(node->prot & VMM_PROT_EXEC)) goto fail;

    if(node->type == VMM_TYPE_ANONYMOUS) {
        if(node->flags & VMM_FLAGS_UNALLOCATED) {
            u64 physical = pmm_alloc_page();
            if(!physical) {
                debug_error("failed to allocate physical page for VMM node");
                goto fail;
            }

            if(!arch_map_page(vas->arch_page_tables, virtual & ~PAGE_MASK, physical, node->prot)) {
                pmm_free_page(physical);
                debug_error("failed to map physical page for VMM node");
                goto fail;
            }

            if(node->page_count == 1) {
                node->flags &= ~VMM_FLAGS_UNALLOCATED;
                node->backing = physical;
                arch_spinlock_release(&vas->lock);
                return 0;
            }

            VMMTreeNode new_node;
            memset(&new_node, 0, sizeof(VMMTreeNode));
            new_node.base = virtual & ~PAGE_MASK;
            new_node.page_count = 1;
            new_node.prot = node->prot;
            new_node.type = VMM_TYPE_ANONYMOUS;
            new_node.flags = 0;
            new_node.backing = physical;
            VMMTreeNode *res = vmm_create_node(vas, &new_node);
            if(!res) {
                pmm_free_page(physical);
                debug_error("failed to create new VMM node during split");
                goto fail;
            }

            arch_spinlock_release(&vas->lock);
            return 0;
        }

        goto unhandled;
    }

    if(node->type == VMM_TYPE_DEVICE) {
        usize pages_into_node = (virtual - node->base) / PAGE_SIZE;
        uptr physical = node->backing + pages_into_node * PAGE_SIZE;

        if(!arch_map_page(vas->arch_page_tables, virtual & ~PAGE_MASK, physical, node->prot)) {
            debug_error("failed to map device page for VMM node");
            goto fail;
        }

        arch_set_uncacheable(vas->arch_page_tables, virtual & ~PAGE_MASK);

        arch_spinlock_release(&vas->lock);
        return 0;
    }

unhandled:
    debug_error("unhandled page fault @ 0x%llX", virtual);
    debug_error("erroneous node:");
    vmm_debug_node(node, 0);

fail:
    arch_spinlock_release(&vas->lock);
    return -1;
}

void *vmm_create_mmio(VASpace *vas, u64 physical, usize size, u16 prot) {
    if(!vas) {
        vas = &kvmm;
    }

    if(!vas->root || !size) {
        return NULL;
    }

    usize page_count = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    u64 base = ARCH_MMIO_BASE;
    u64 limit = (u64) -1;

    arch_spinlock_acquire(&vas->lock);
    arch_switch_page_tables(vas->arch_page_tables);

    VMMTreeNode *parent = vmm_lenient_search(vas->root, base);
    if(!parent) {
        goto allocate;
    }

    if(parent->children_count == 0) {
        parent = parent->parent;
    }

    u64 addr = base;
    while(addr + page_count * PAGE_SIZE <= limit) {
        int conflict = 0;
        for(u16 i = 0; i < parent->children_count; i++) {
            VMMTreeNode *child = parent->children[i];

            if(!((addr + page_count * PAGE_SIZE <= child->base)
                || (addr >= child->max_virtual_address))) {
                conflict = 1;
                addr = child->max_virtual_address;
                break;
            }
        }

        if(conflict) {
            continue;
        }

        break;
    }

    if(addr + page_count * PAGE_SIZE > limit) {
        arch_spinlock_release(&vas->lock);
        return NULL;
    }

    base = addr;

allocate:
    VMMTreeNode new_node;
    memset(&new_node, 0, sizeof(VMMTreeNode));
    new_node.base = base;
    new_node.page_count = page_count;
    new_node.prot = prot;
    new_node.type = VMM_TYPE_DEVICE;
    new_node.flags = 0;
    new_node.backing = physical;

    VMMTreeNode *res = vmm_create_node(vas, &new_node);
    if(!res) {
        arch_spinlock_release(&vas->lock);
        return NULL;
    }

    arch_spinlock_release(&vas->lock);
    return (void *) res->base;
}
