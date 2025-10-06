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

#include <kiwi/vmm.h>
#include <kiwi/arch/atomic.h>

typedef struct HeapHeader {
    u64 size;
    struct HeapHeader *next;
    u64 free;
    u64 padding;
} HeapHeader;

static HeapHeader *heap_start = NULL;
static HeapHeader *heap_end = NULL;
static usize heap_total_size = 0;
static lock_t heap_lock = LOCK_INITIAL;

void *malloc(size_t size) {
    if(size == 0) {
        return NULL;
    }

    if(size % sizeof(HeapHeader)) {
        size += sizeof(HeapHeader) - (size % sizeof(HeapHeader));
    }

    usize total_size = size + sizeof(HeapHeader);
    arch_spinlock_acquire(&heap_lock);

    if(!heap_start) {
        usize page_count = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;
        heap_start = vmm_allocate(NULL, ARCH_KERNEL_HEAP_BASE, (u64) -1, page_count, VMM_PROT_READ | VMM_PROT_WRITE);
        if(!heap_start) {
            arch_spinlock_release(&heap_lock);
            return NULL;
        }

        heap_start->size = size;
        heap_start->next = NULL;
        heap_start->free = 0;

        heap_end = (HeapHeader *) ((uptr) heap_start + page_count * PAGE_SIZE);
        heap_total_size = page_count * PAGE_SIZE;
        arch_spinlock_release(&heap_lock);
        return (void *) ((uptr) heap_start + sizeof(HeapHeader));
    }

    HeapHeader *current = heap_start;
    while(((uptr) current < (uptr) heap_end)) {
        if(current->free && current->size >= size) {
            current->free = 0;
            arch_spinlock_release(&heap_lock);
            return (void *) ((uptr) current + sizeof(HeapHeader));
        }

        if(!current->next) {
            break;
        }

        current = current->next;
    }

    uptr remaining_size = (uptr) heap_end - (uptr) current
        - sizeof(HeapHeader) - current->size;

    if(remaining_size >= total_size) {
        HeapHeader *next = (HeapHeader *) ((uptr) current + sizeof(HeapHeader) + current->size);
        next->size = size;
        next->next = NULL;
        next->free = 0;
        current->next = next;
        arch_spinlock_release(&heap_lock);
        return (void *) ((uptr) next + sizeof(HeapHeader));
    }

    usize page_count = (total_size - remaining_size + PAGE_SIZE - 1) / PAGE_SIZE;

    void *new_block = vmm_allocate(NULL, (uptr) heap_end, (u64) -1,
        page_count, VMM_PROT_READ | VMM_PROT_WRITE);

    if(!new_block) {
        arch_spinlock_release(&heap_lock);
        return NULL;
    }

    heap_total_size += page_count * PAGE_SIZE;
    heap_end = (HeapHeader *) ((uptr) new_block + page_count * PAGE_SIZE);
    current->next = (HeapHeader *) ((uptr) current + sizeof(HeapHeader) + current->size);

    current = current->next;
    current->size = size;
    current->next = NULL;
    current->free = 0;
    arch_spinlock_release(&heap_lock);
    return (void *) ((uptr) current + sizeof(HeapHeader));
}
