/*
 * kiwi - general-purpose high-performance operating system
 * 
 * Copyright (c) 2026 Omar Elghoul
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

#include <kiwi/structs/priorityqueue.h>
#include <stdlib.h>

#define PQ_INITIAL_CAPACITY          4

PriorityQueue *pq_create(void) {
    PriorityQueue *pq = malloc(sizeof(PriorityQueue));
    if(!pq)
        return NULL;

    pq->nodes = malloc(sizeof(PriorityQueueNode) * PQ_INITIAL_CAPACITY);
    if(!pq->nodes) {
        free(pq);
        return NULL;
    }

    pq->capacity = PQ_INITIAL_CAPACITY;
    pq->size = 0;
    return pq;
}

void pq_destroy(PriorityQueue *pq) {
    if(pq) {
        if(pq->nodes)
            free(pq->nodes);
        free(pq);
    }
}

int pq_push(PriorityQueue *pq, u64 priority, void *data) {
    usize idx, parent, new_capacity;
    PriorityQueueNode *new_nodes;
    PriorityQueueNode node;
    if(!pq)
        return -1;

    if(pq->size == pq->capacity) {
        new_capacity = pq->capacity * 2;
        new_nodes = realloc(pq->nodes, new_capacity * sizeof(*pq->nodes));
        if(!new_nodes)
            return -1;

        pq->nodes = new_nodes;
        pq->capacity = new_capacity;
    }

    idx = pq->size++;
    node.priority = priority;
    node.data = data;
    while(idx > 0) {
        parent = (idx - 1) / 2;

        if(pq->nodes[parent].priority <= priority) /* min-heap */
            break;

        pq->nodes[idx] = pq->nodes[parent];
        idx = parent;
    }

    pq->nodes[idx] = node;
    return 0;
}

void *pq_pop(PriorityQueue *pq, u64 *priority) {
    usize idx, left, right, smallest;
    PriorityQueueNode root, last;
    if(!pq || pq->size == 0)
        return NULL;

    root = pq->nodes[0];
    last = pq->nodes[--pq->size];
    idx = 0;

    while(1) {
        left = 2 * idx + 1;
        right = left + 1;
        if(left >= pq->size)
            break;

        smallest = left;
        if(right < pq->size &&
            pq->nodes[right].priority < pq->nodes[left].priority)
            smallest = right;

        if(pq->nodes[smallest].priority >= last.priority)
            break;

        pq->nodes[idx] = pq->nodes[smallest];
        idx = smallest;
    }

    if(pq->size > 0)
        pq->nodes[idx] = last;
    if(priority)
        *priority = root.priority;

    return root.data;
}

void *pq_peek(PriorityQueue *pq, u64 *priority) {
    if(!pq || pq->size == 0)
        return NULL;

    if(priority)
        *priority = pq->nodes[0].priority;

    return pq->nodes[0].data;
}
