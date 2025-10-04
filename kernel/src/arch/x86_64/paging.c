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

#include <kiwi/arch/x86_64.h>
#include <kiwi/arch/paging.h>
#include <kiwi/arch/memmap.h>
#include <kiwi/boot.h>
#include <kiwi/pmm.h>
#include <kiwi/vmm.h>
#include <kiwi/debug.h>
#include <kiwi/tty.h>
#include <string.h>

static uptr *kernel_paging_root;
static uptr hhdm_base = 0;

uptr arch_map_page(uptr cr3, uptr virtual, uptr physical, u16 prot) {
    int pml4_index = (virtual >> 39) & 0x1FF;
    int pdp_index = (virtual >> 30) & 0x1FF;
    int pd_index = (virtual >> 21) & 0x1FF;
    int pt_index = (virtual >> 12) & 0x1FF;

    uptr *pml4, *pdp, *pd, *pt;

    pml4 = (uptr *) ((cr3 & ~PAGE_MASK) + hhdm_base);
    if(!(pml4[pml4_index] & PAGE_PRESENT)) {
        uptr new_pdp = pmm_alloc_page();
        if(!new_pdp) goto no_memory;
        pml4[pml4_index] = ((uptr) new_pdp)
            | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
        pdp = (uptr *) (new_pdp + hhdm_base);
        memset(pdp, 0, PAGE_SIZE);
    } else {
        pdp = (uptr *) ((pml4[pml4_index] & ~PAGE_MASK) + hhdm_base);
    }

    if(!(pdp[pdp_index] & PAGE_PRESENT)) {
        uptr new_pd = pmm_alloc_page();
        if(!new_pd) goto no_memory;
        pdp[pdp_index] = ((uptr) new_pd)
            | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
        pd = (uptr *) (new_pd + hhdm_base);
        memset(pd, 0, PAGE_SIZE);
    } else {
        pd = (uptr *) ((pdp[pdp_index] & ~PAGE_MASK) + hhdm_base);
    }

    if(!(pd[pd_index] & PAGE_PRESENT)) {
        uptr new_pt = pmm_alloc_page();
        if(!new_pt) goto no_memory;
        pd[pd_index] = ((uptr) new_pt)
            | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
        pt = (uptr *) (new_pt + hhdm_base);
        memset(pt, 0, PAGE_SIZE);
    } else {
        pt = (uptr *) ((pd[pd_index] & ~PAGE_MASK) + hhdm_base);
    }
    
    uptr new_entry = (physical & ~PAGE_MASK) | PAGE_PRESENT;
    if(prot & VMM_PROT_WRITE) new_entry |= PAGE_WRITABLE;
    if(prot & VMM_PROT_USER) new_entry |= PAGE_USER;

    pt[pt_index] = new_entry;
    return virtual;

no_memory:
    return 0;
}

uptr arch_map_large_page(uptr cr3, uptr virtual, uptr physical, u16 prot) {
    int pml4_index = (virtual >> 39) & 0x1FF;
    int pdp_index = (virtual >> 30) & 0x1FF;
    int pd_index = (virtual >> 21) & 0x1FF;

    uptr *pml4, *pdp, *pd;

    pml4 = (uptr *) ((cr3 & ~PAGE_MASK) + hhdm_base);
    if(!(pml4[pml4_index] & PAGE_PRESENT)) {
        uptr new_pdp = pmm_alloc_page();
        if(!new_pdp) goto no_memory;
        pml4[pml4_index] = ((uptr) new_pdp)
            | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
        pdp = (uptr *) (new_pdp + hhdm_base);
        memset(pdp, 0, PAGE_SIZE);
    } else {
        pdp = (uptr *) ((pml4[pml4_index] & ~PAGE_MASK) + hhdm_base);
    }

    if(!(pdp[pdp_index] & PAGE_PRESENT)) {
        uptr new_pd = pmm_alloc_page();
        if(!new_pd) goto no_memory;
        pdp[pdp_index] = ((uptr) new_pd)
            | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
        pd = (uptr *) (new_pd + hhdm_base);
        memset(pd, 0, PAGE_SIZE);
    } else {
        pd = (uptr *) ((pdp[pdp_index] & ~PAGE_MASK) + hhdm_base);
    }
    
    uptr new_entry = (physical & ~PAGE_MASK) | PAGE_PRESENT | PAGE_SIZE_TOGGLE;
    if(prot & VMM_PROT_WRITE) new_entry |= PAGE_WRITABLE;
    if(prot & VMM_PROT_USER) new_entry |= PAGE_USER;

    pd[pd_index] = new_entry;
    return virtual;

no_memory:
    return 0;
}

int arch_unmap_page(uptr cr3, uptr virtual) {
    int pml4_index = (virtual >> 39) & 0x1FF;
    int pdp_index = (virtual >> 30) & 0x1FF;
    int pd_index = (virtual >> 21) & 0x1FF;
    int pt_index = (virtual >> 12) & 0x1FF;

    uptr *pml4, *pdp, *pd, *pt;

    pml4 = (uptr *) ((cr3 & ~PAGE_MASK) + hhdm_base);
    if(!(pml4[pml4_index] & PAGE_PRESENT)) return -1;
    pdp = (uptr *) ((pml4[pml4_index] & ~PAGE_MASK) + hhdm_base);

    if(!(pdp[pdp_index] & PAGE_PRESENT)) return -1;
    pd = (uptr *) ((pdp[pdp_index] & ~PAGE_MASK) + hhdm_base);

    if(!(pd[pd_index] & PAGE_PRESENT)) return -1;
    pt = (uptr *) ((pd[pd_index] & ~PAGE_MASK) + hhdm_base);

    if(!(pt[pt_index] & PAGE_PRESENT)) return -1;

    pt[pt_index] = 0;
    return 0;
}

int arch_get_page(uptr cr3, uptr virtual, uptr *physical, u16 *prot) {
    int pml4_index = (virtual >> 39) & 0x1FF;
    int pdp_index = (virtual >> 30) & 0x1FF;
    int pd_index = (virtual >> 21) & 0x1FF;
    int pt_index = (virtual >> 12) & 0x1FF;

    debug_info("attempt to get page info for VA=0x%llX", virtual);

    uptr *pml4, *pdp, *pd, *pt;
    u64 entry;

    pml4 = (uptr *) ((cr3 & ~PAGE_MASK) + hhdm_base);
    if(!(pml4[pml4_index] & PAGE_PRESENT)) return -1;

    pdp = (uptr *) ((pml4[pml4_index] & ~PAGE_MASK) + hhdm_base);
    if(!(pdp[pdp_index] & PAGE_PRESENT)) return -1;

    pd = (uptr *) ((pdp[pdp_index] & ~PAGE_MASK) + hhdm_base);
    if(!(pd[pd_index] & PAGE_PRESENT)) return -1;
    if(pd[pd_index] & PAGE_SIZE_TOGGLE) {
        entry = pd[pd_index];
        goto done;
    }

    pt = (uptr *) ((pd[pd_index] & ~PAGE_MASK) + hhdm_base);
    if(!(pt[pt_index] & PAGE_PRESENT)) return -1;
    entry = pt[pt_index];

done:
    if(physical) {
        *physical = entry & ~PAGE_MASK;
    }

    if(prot) {
        *prot = 0;
        if(entry & PAGE_WRITABLE) *prot |= VMM_PROT_WRITE;
        if(entry & PAGE_USER) *prot |= VMM_PROT_USER;
        *prot |= VMM_PROT_READ;
    }

    return 0;
}

uptr arch_paging_init(void) {
    kernel_paging_root = (uptr *) pmm_alloc_page();
    if(!kernel_paging_root) goto no_memory;

    memset(kernel_paging_root, 0, 4096);

    usize hhdm_pages = (usize) (pmm.highest_address + LARGE_PAGE_SIZE - 1)
        / LARGE_PAGE_SIZE;
    for(usize i = 0; i < hhdm_pages; i++) {
        if(!arch_map_large_page((uptr) kernel_paging_root,
                ARCH_HHDM_BASE + i * LARGE_PAGE_SIZE,
                i * LARGE_PAGE_SIZE,
                VMM_PROT_READ | VMM_PROT_WRITE)) {
            goto no_memory;
        }
    }

    debug_info("mapped %llu MB of memory in the HHDM",
        (hhdm_pages * LARGE_PAGE_SIZE) >> 20);

    usize kernel_pages = (kiwi_boot_info.lowest_free_address + LARGE_PAGE_SIZE - 1)
        / LARGE_PAGE_SIZE;

    for(usize i = 0; i < kernel_pages; i++) {
        if(!arch_map_large_page((uptr) kernel_paging_root,
                ARCH_KERNEL_IMAGE_BASE + i * LARGE_PAGE_SIZE,
                i * LARGE_PAGE_SIZE,
                VMM_PROT_READ | VMM_PROT_WRITE)) {
            goto no_memory;
        }
    }

    debug_info("mapped %llu MB of memory for the kernel",
        (kernel_pages * LARGE_PAGE_SIZE) >> 20);

    arch_set_cr3((uptr) kernel_paging_root);

    hhdm_base = ARCH_HHDM_BASE;
    if(kernel_terminal.front_buffer) {
        kernel_terminal.front_buffer += hhdm_base / sizeof(u32);
    }

    if(kernel_terminal.back_buffer) {
        kernel_terminal.back_buffer += hhdm_base / sizeof(u32);
    }

    pmm.bitmap_start += hhdm_base;
    return (uptr) kernel_paging_root;

no_memory:
    debug_panic("unable to allocate memory for kernel page tables");
    for(;;);
}

void arch_switch_page_tables(uptr page_tables) {
    arch_set_cr3(page_tables);
}
