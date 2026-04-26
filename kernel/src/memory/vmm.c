/*
 * kiwi - general-purpose high-performance operating system
 * 
 * Copyright (c) 2025-26 Omar Elghoul
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

#define INITIAL_CHILDREN_CAPACITY   4
VASpace kvmm;

/* these helper functions provide a mini-malloc-like allocator that the vmm
 * can use for its internal nodes and other metadata. this is necessary to
 * avoid circular dependencies
 */

struct vmm_metadata_header {
    usize size;
    struct vmm_metadata_header *next, *prev;
    int used;
};


static lock_t vmm_metadata_lock = LOCK_INITIAL;
static struct vmm_metadata_header *vmm_metadata_base = NULL;
static size_t vmm_metadata_size_pages = 0;

/* vmm_metadata_shrink: shrinks the metadata pool
 * in: pages - number of pages to shrink by
 * out: nothing
 * notes: this function must be called with vmm_metadata_lock held
 */

static void vmm_metadata_shrink(usize pages) {
    uptr virtual, physical;
    if(!pages || pages > vmm_metadata_size_pages) {
        return;
    }

    for(usize i = 0; i < pages; i++) {
        virtual = (uptr) vmm_metadata_base + ((vmm_metadata_size_pages - 1 - i)
            * PAGE_SIZE);

        if(arch_get_page((uptr) kvmm.arch_page_tables, virtual, &physical, NULL) < 0) {
            continue;
        }

        arch_unmap_page((uptr) kvmm.arch_page_tables, virtual);
        pmm_free_page(physical);
    }
}

/* vmm_metadata_expand: expands the metadata pool
 * in: pages - number of pages to expand by
 * out: 0 on success, negative error code on failure
 * notes: this function must be called with vmm_metadata_lock held
 */

static int vmm_metadata_expand(usize pages) {
    uptr virtual, physical;

    if(!pages)
        return -1;

    /* for the first allocation we want to start at this fixed base */
    if(!vmm_metadata_base)
        vmm_metadata_base = (struct vmm_metadata_header *) ARCH_VMM_BASE;

    for(usize i = 0; i < pages; i++) {
        physical = pmm_alloc_page();
        if(!physical) {
            vmm_metadata_shrink(i);
            return -1;
        }

        virtual = (uptr) vmm_metadata_base + vmm_metadata_size_pages * PAGE_SIZE;
        if(!arch_map_page((uptr) kvmm.arch_page_tables, virtual, physical,
            VMM_PROT_READ | VMM_PROT_WRITE)) {
            pmm_free_page(physical);
            vmm_metadata_shrink(i);
            return -1;
        }

        vmm_metadata_size_pages++;
    }

    return 0;
}

/* vmm_metadata_alloc: allocates memory for vmm metadata
 * in: size - number of bytes to allocate
 * out: pointer to the allocated memory on success, NULL on failure
 */

void *vmm_metadata_alloc(usize size) {
    usize adjusted_sz = size + sizeof(struct vmm_metadata_header);
    usize adjusted_sz_pages = ROUND_TO_PAGES(adjusted_sz);
    struct vmm_metadata_header *header, *prev, *current;
    void *ptr = NULL;
    int status;

    if(!size)
        goto out;

    arch_spinlock_acquire(&vmm_metadata_lock);

    if(!vmm_metadata_base) {
        status = vmm_metadata_expand(adjusted_sz_pages);
        if(status)
            goto release_and_out;

        header = vmm_metadata_base;
        header->size = size;
        header->next = NULL;
        header->prev = NULL;
        header->used = 1;
        ptr = (void *) (header + 1);
        goto release_and_out;
    }

    current = vmm_metadata_base;
    prev = NULL;
    while(current) {
        if(!current->used && current->size >= size) {
            current->used = 1;
            ptr = (void *) (current + 1);
            goto release_and_out;
        }

        prev = current;
        current = current->next;
    }

    /* no suitable block found, need to expand */
    status = vmm_metadata_expand(adjusted_sz_pages);
    if(status)
        goto release_and_out;
    
    header = (struct vmm_metadata_header *) ((uptr) prev
        + sizeof(struct vmm_metadata_header) + prev->size);
    header->size = size;
    header->next = NULL;
    header->prev = prev;
    header->used = 1;
    prev->next = header;
    ptr = (void *) (header + 1);

release_and_out:
    arch_spinlock_release(&vmm_metadata_lock);
out:
    if(!ptr)
        debug_panic("cannot allocate memory for vmm metadata");
    memset(ptr, 0, size);
    return ptr;
}

static void vmm_metadata_free(void *ptr) {
    struct vmm_metadata_header *header;
    usize pages_to_shrink;

    if(!ptr)
        return;

    arch_spinlock_acquire(&vmm_metadata_lock);

    header = (struct vmm_metadata_header *) ptr - 1;
    header->used = 0;
    if(!header->next) {
        if((uptr) header & PAGE_MASK) {
            /* header is not on a page boundary, so we will free from the NEXT
             * page and re-adjust the size in this header */
            pages_to_shrink = vmm_metadata_size_pages
                - ((uptr) header - ARCH_VMM_BASE) / PAGE_SIZE - 1;
            header->size = PAGE_SIZE - ((uptr) header % PAGE_SIZE)
                - sizeof(struct vmm_metadata_header);
        } else {
            /* header is on a page boundary, so we can free from this page but
             * we will need to adjust the next pointer of the previous header */
            pages_to_shrink = vmm_metadata_size_pages
                - ((uptr) header - ARCH_VMM_BASE) / PAGE_SIZE;
            if(header->prev) {
                header->prev->next = NULL;
            }
        }

        vmm_metadata_shrink(pages_to_shrink);
    }

    arch_spinlock_release(&vmm_metadata_lock);
}

static void *vmm_metadata_realloc(void *ptr, usize size) {
    /* this is soooo lazy but I will improve it someday trust */
    void *new_ptr = vmm_metadata_alloc(size);
    if(ptr && new_ptr) {
        memcpy(new_ptr, ptr, size);
        vmm_metadata_free(ptr);
    }

    return new_ptr;
}

/*
 * actual VMM implementation starts here
 */

/* vmm_search: searches for the deepest node containing a virtual address
 * in: root - root of the VMM tree
 *     virtual - the virtual address to search for
 * out: pointer to the node containing the virtual address on success, NULL if not found
 * notes: the lock of the corresponding VASpace is held by the caller
 */

static VMMTreeNode *vmm_search(VMMTreeNode *root, u64 virtual) {
    VMMTreeNode *child;

    if(!root)
        return NULL;

    if((virtual >= root->base) && (virtual <= root->limit)) {
        if(root->type == VMM_TYPE_INTERNAL) {
            for(u64 i = 0; i < root->children_count; i++) {
                child = root->children[i];
                if(child && (virtual >= child->base) && (virtual <= child->limit)) {
                    return vmm_search(child, virtual);
                }
            }
        }

        return root;
    }

    return NULL;
}

/* vmm_propagate: propagates changes in the VMM tree
 * in: root - root of the VMM tree
 * in: node - the (leaf) node to propagate changes from
 * out: nothing
 * notes: the lock of the corresponding VASpace is held by the caller
 */

static void vmm_propagate(VMMTreeNode *root, VMMTreeNode *node) {
    VMMTreeNode *child;

    if(!root || !node)
        return;    
    if((root == node) || (root->type != VMM_TYPE_INTERNAL))
        return;

    if(root->base > node->base)
        root->base = node->base;
    if(root->limit < node->limit)
        root->limit = node->limit;

    for(u64 i = 0; i < root->children_count; i++) {
        child = root->children[i];
        if(child && (child != node))
            vmm_propagate(child, node);
    }
}

/* vmm_insert_node: inserts a new node into the vmm tree
 * in: root - root of the VMM tree
 *     new_node - template node to be inserted
 * out: ptr to the inserted node on success, NULL on failure
 * notes: the lock of the corresponding VASpace is held by the caller
 */

static VMMTreeNode *vmm_insert_node(VMMTreeNode *root,
                                    const VMMTreeNode *new_node) {
    VMMTreeNode *node = vmm_metadata_alloc(sizeof(VMMTreeNode));
    VMMTreeNode *parent = vmm_search(root, new_node->base);
    VMMTreeNode **new_children;
    usize new_capacity;

    if(!parent)     /* in this case we can add directly under the root */
        parent = root;

    memcpy(node, new_node, sizeof(VMMTreeNode));
    node->parent = parent;

    if(parent->children_count == parent->max_children_count) {
        new_capacity = parent->max_children_count
            ? parent->max_children_count * 2 : INITIAL_CHILDREN_CAPACITY;
        new_children = vmm_metadata_realloc(parent->children,
            new_capacity * sizeof(VMMTreeNode *));

        parent->children = new_children;
        parent->max_children_count = new_capacity;
    }

    parent->children[parent->children_count++] = node;
    vmm_propagate(root, node);
    return node;
}

/* vmm_allocate: allocates and maps contiguous pages
 * in: vas - virtual address space
 *     base - base virtual address to start searching from
 *     limit - limit virtual address to search up to
 *     page_count - number of contiguous pages to allocate
 *     prot - protection flags for the mapping (VMM_PROT_*)
 *     strict_range - if nonzero, only allocate within the [base, limit) range
 * out: pointer to the allocated virtual memory on success, NULL on failure
 */

void *vmm_allocate(VASpace *vas, u64 base, u64 limit, usize page_count,
                   u16 prot, int strict_range) {
    VMMTreeNode *root, *parent, *child, new_node = {0};
    u64 actual_base = base;
    void *ptr = NULL;
    int conflicted;

    if(!vas)
        goto out;

    if(IS_KERNEL_ADDRESS(base) || IS_KERNEL_ADDRESS(limit))
        root = kvmm.root;
    else
        root = vas->root;

    arch_spinlock_acquire(&vas->lock);

    parent = vmm_search(root, base);
    if(!parent)
        goto allocate_node;
    if(parent->type != VMM_TYPE_INTERNAL)
        goto release_and_out;

    do {
        conflicted = 0;
        for(u64 i = 0; i < parent->children_count; i++) {
            child = parent->children[i];
            if(child && (actual_base + page_count * PAGE_SIZE - 1 >= child->base)
                && (actual_base <= child->limit)) {
                conflicted = 1;
                actual_base = child->limit + 1;
                break;
            }
        }

        if(conflicted && strict_range)
            goto release_and_out;
    } while(conflicted && (actual_base + page_count * PAGE_SIZE - 1 <= parent->limit));

    if(actual_base + page_count * PAGE_SIZE > limit)
        goto release_and_out;

allocate_node:
    new_node.base = actual_base;
    new_node.limit = actual_base + page_count * PAGE_SIZE - 1;
    new_node.prot = prot;
    new_node.type = VMM_TYPE_ANONYMOUS;
    /* so the page fault handlers knows to allocate physical pages */
    new_node.flags = VMM_FLAGS_UNALLOCATED;
    vmm_insert_node(root, &new_node);
    ptr = (void *) actual_base;

release_and_out:
    arch_spinlock_release(&vas->lock);
out:
    return ptr;
}

/* vmm_create_mmio: creates a memory-mapped I/O region
 * in: vas - virtual address space
 *     physical - base physical address
 *     size - size of the region in pages
 *     prot - protection flags for the mapping (VMM_PROT_*)
 * out: pointer to the allocated virtual memory on success, NULL on failure
 */

void *vmm_create_mmio(VASpace *vas, u64 physical, usize size, u16 prot) {
    VMMTreeNode *root, *parent, *child, new_node = {0};
    u64 actual_base = ARCH_MMIO_BASE;
    void *ptr = NULL;
    int conflicted;

    if(!vas)
        goto out;

    root = kvmm.root;
    arch_spinlock_acquire(&vas->lock);

    parent = vmm_search(root, actual_base);
check_parent:
    if(!parent)
        goto allocate_node;
    if(parent->type != VMM_TYPE_INTERNAL) {
        parent = parent->parent;
        goto check_parent;
    }

    do {
        conflicted = 0;
        for(u64 i = 0; i < parent->children_count; i++) {
            child = parent->children[i];
            if(child && (actual_base + size - 1 >= child->base)
                && (actual_base <= child->limit)) {
                conflicted = 1;
                actual_base = child->limit + 1;
                break;
            }
        }
    } while(conflicted && (actual_base + size - 1 <= parent->limit));

    if(actual_base + size > ARCH_MMIO_LIMIT)
        goto release_and_out;

allocate_node:
    new_node.base = actual_base;
    new_node.limit = actual_base + size * PAGE_SIZE - 1;
    new_node.prot = prot;
    new_node.type = VMM_TYPE_DEVICE;
    new_node.backing = physical;
    vmm_insert_node(root, &new_node);
    ptr = (void *) actual_base;

release_and_out:
    arch_spinlock_release(&vas->lock);
out:
    return ptr;
}

/* vmm_page_fault: generic page fault handler
 * in: vas - the virtual address space in which the fault occurred
 *     virtual - the faulting virtual address
 *     user - whether the fault occurred in user mode
 *     write - whether the fault was caused by a write
 *     exec - whether the fault was caused by an instruction fetch
 * out: 0 on success, negative error code on failure
 */

int vmm_page_fault(VASpace *vas, u64 virtual, int user, int write, int exec) {
    VMMTreeNode *node, *root;
    int status = -1;
    uptr physical;
    uptr aligned_virtual = virtual & ~PAGE_MASK;
    uptr page_index;

    arch_spinlock_acquire(&vas->lock);

    if(IS_KERNEL_ADDRESS(virtual))
        root = kvmm.root;
    else
        root = vas->root;

    node = vmm_search(root, virtual);
    if(!node)
        goto release_and_out;
    if(user && !(node->prot & VMM_PROT_USER))
        goto release_and_out;
    if(write && !(node->prot & VMM_PROT_WRITE))
        goto release_and_out;
    if(exec && !(node->prot & VMM_PROT_EXEC))
        goto release_and_out;

    switch(node->type) {
    case VMM_TYPE_ANONYMOUS:
        if(node->flags & VMM_FLAGS_UNALLOCATED) {
            physical = pmm_alloc_page();
            if(!physical)
                goto release_and_out;
            
            if(!arch_map_page(vas->arch_page_tables, aligned_virtual, physical,
                node->prot)) {
                pmm_free_page(physical);
                goto release_and_out;
            }

            status = 0;
            break;
        } else {
            debug_error("already allocated but not mapped? this should not happen");
            debug_error("possible tlb issues...???");
            goto release_and_out;
        }
    case VMM_TYPE_DEVICE:
        page_index = (aligned_virtual - node->base) / PAGE_SIZE;
        physical = node->backing + page_index * PAGE_SIZE;

        if(!arch_map_page(vas->arch_page_tables, aligned_virtual, physical,
            node->prot))
            goto release_and_out;
        
        status = 0;
        break;
    default:
        debug_error("unhandled page fault for node type %u", node->type);
        debug_error(" faulting address 0x%llX (user=%d, write=%d, exec=%d)",
            virtual, user, write, exec);
        debug_error(" protection flags: %s%s%s%s",
            (node->prot & VMM_PROT_READ) ? "R" : "",
            (node->prot & VMM_PROT_WRITE) ? "W" : "",
            (node->prot & VMM_PROT_EXEC) ? "X" : "",
            (node->prot & VMM_PROT_USER) ? "U" : "");
        debug_error(" node range: [0x%llX - 0x%llX]", node->base, node->limit);
    }

release_and_out:
    arch_spinlock_release(&vas->lock);
    return status;
}

/* vmm_create_vaspace: creates a new virtual address space
 * in: dest - destination virtual address space
 *     page_tables - pointer to the page tables
 * out: pointer to the created virtual address space on success, NULL on failure
 */

VASpace *vmm_create_vaspace(VASpace *dest, uptr page_tables) {
    return NULL;
}

/* and we're finally here... */

void vmm_init(void) {
    debug_info("building kernel page tables...");
    kvmm.arch_page_tables = arch_paging_init();
    if(!kvmm.arch_page_tables) {
        debug_panic("failed to initialize paging");
        for(;;);
    }

    debug_info("creating kernel vmm nodes...");
    kvmm.root = vmm_metadata_alloc(sizeof(VMMTreeNode));
    kvmm.root->base = ARCH_MIN_KERNEL_ADDRESS;
    kvmm.root->limit = ARCH_MAX_KERNEL_ADDRESS;
    kvmm.root->type = VMM_TYPE_INTERNAL;

    VMMTreeNode hhdm_node = {0};
    hhdm_node.base = ARCH_HHDM_BASE;
    hhdm_node.limit = ARCH_HHDM_BASE + pmm.highest_address - 1;
    hhdm_node.prot = VMM_PROT_READ | VMM_PROT_WRITE;
    hhdm_node.type = VMM_TYPE_ANONYMOUS;
    vmm_insert_node(kvmm.root, &hhdm_node);

    VMMTreeNode kernel_node = {0};
    kernel_node.base = ARCH_KERNEL_IMAGE_BASE;
    kernel_node.limit = ARCH_KERNEL_IMAGE_BASE + kiwi_boot_info.lowest_free_address - 1;
    kernel_node.prot = VMM_PROT_READ | VMM_PROT_WRITE | VMM_PROT_EXEC;
    kernel_node.type = VMM_TYPE_ANONYMOUS;
    vmm_insert_node(kvmm.root, &kernel_node);
}