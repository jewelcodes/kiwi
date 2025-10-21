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

#include <kiwi/structs/cldeque.h>
#include <kiwi/arch/atomic.h>
#include <stdlib.h>
#include <string.h>

// reference:
// https://www.dre.vanderbilt.edu/~schmidt/PDF/work-stealing-dequeue.pdf

#define CLDEQUE_INITIAL_CAPACITY        32

CLDeque *cldeque_create(void) {
    CLDeque *deque = malloc(sizeof(CLDeque));
    if(!deque) {
        return NULL;
    }

    deque->items = calloc(CLDEQUE_INITIAL_CAPACITY, sizeof(u64));
    if(!deque->items) {
        free(deque);
        return NULL;
    }

    deque->capacity = CLDEQUE_INITIAL_CAPACITY;
    deque->head = 0;
    deque->tail = 0;
    return deque;
}

void cldeque_destroy(CLDeque *deque) {
    if(deque) {
        if(deque->items) {
            free(deque->items);
        }
        free(deque);
    }
}

int cldeque_push(CLDeque *deque, u64 item) {
    if(!deque) {
        return -1;
    }

    u64 size = (deque->tail > deque->head)
        ? deque->tail - deque->head
        : deque->capacity - (deque->head - deque->tail);

    if(size >= deque->capacity - 1) {
        // we are deliberately not using realloc here to avoid issues with
        // concurrent access to the old items pointer, which may be freed.
        // this function is NOT thread-safe, but it should NOT break concurrent
        // steal operations either.

        u64 new_capacity = deque->capacity * 2;
        u64 old_capacity = deque->capacity;
        u64 *old_items = deque->items;
        u64 *new_items = calloc(new_capacity, sizeof(u64));
        if(!new_items) {
            return -1;
        }

        memcpy(new_items, old_items, sizeof(u64) * old_capacity);
        deque->items = new_items;
        deque->capacity = new_capacity;
        free(old_items);
    }

    deque->items[deque->tail] = item;
    deque->tail++;
    if(deque->tail >= deque->capacity) {
        deque->tail = deque->tail % deque->capacity;
    }
    return 0;
}

int cldeque_pop(CLDeque *deque, u64 *item) {
    if(!deque) {
        return -1;
    }

    u64 size = (deque->tail > deque->head)
        ? deque->tail - deque->head
        : deque->capacity - (deque->head - deque->tail);

    if(!size) {
        return -1;
    }

    if(item) {
        *item = deque->items[deque->tail - 1];
    }

    deque->tail--;
    if(deque->tail >= deque->capacity) {
        deque->tail = deque->tail % deque->capacity;
    }
    return 0;
}

int cldeque_steal(CLDeque *deque, u64 *item) {
    if(!deque) {
        return -1;
    }

    u64 size = (deque->tail > deque->head)
        ? deque->tail - deque->head
        : deque->capacity - (deque->head - deque->tail);
    if(!size) {
        return -1;
    }

    u64 stolen_item = deque->items[deque->head];
    u64 new_head = deque->head + 1;
    if(new_head >= deque->capacity) {
        new_head = new_head % deque->capacity;
    }

    if(!arch_cas64(&deque->head, deque->head, new_head)) {
        return -1;
    }

    if(item) {
        *item = stolen_item;
    }

    return 0;
}
