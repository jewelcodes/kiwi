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

#include <kiwi/structs/array.h>
#include <stdlib.h>

#define ARRAY_INITIAL_CAPACITY          4

Array *array_create(void) {
    Array *array = malloc(sizeof(Array));
    if(!array) {
        return NULL;
    }

    array->items = malloc(sizeof(u64) * ARRAY_INITIAL_CAPACITY);
    if(!array->items) {
        free(array);
        return NULL;
    }

    array->count = 0;
    array->capacity = ARRAY_INITIAL_CAPACITY;
    return array;
}

void array_destroy(Array *array) {
    if(array) {
        if(array->items) {
            free(array->items);
        }
        free(array);
    }
}

int array_push(Array *array, u64 item) {
    if(array->count == array->capacity) {
        u64 new_capacity = array->capacity * 2;
        u64 *new_items = (u64 *) realloc(array->items, sizeof(u64) * new_capacity);
        if(!new_items) {
            return -1;
        }
        array->items = new_items;
        array->capacity = new_capacity;
    }
    array->items[array->count++] = item;
    return 0;
}

int array_pop_back(Array *array, u64 *item) {
    if(array->count == 0) {
        return -1;
    }

    u64 popped_item = array->items[--array->count];
    if((array->count > 0) && (array->count <= (array->capacity / 4))
        && ((array->capacity / 2) >= ARRAY_INITIAL_CAPACITY)) {
        u64 new_capacity = array->capacity / 2;
        u64 *new_items = (u64 *) realloc(array->items, sizeof(u64) * new_capacity);
        if(new_items) {
            array->items = new_items;
            array->capacity = new_capacity;
        }
    }

    *item = popped_item;
    return 0;
}

int array_pop_front(Array *array, u64 *item) {
    if(array->count == 0) {
        return -1;
    }

    u64 popped_item = array->items[0];
    for(u64 i = 1; i < array->count; i++) {
        array->items[i - 1] = array->items[i];
    }
    array->count--;

    if((array->count > 0) && (array->count <= (array->capacity / 4))
        && ((array->capacity / 2) >= ARRAY_INITIAL_CAPACITY)) {
        u64 new_capacity = array->capacity / 2;
        u64 *new_items = (u64 *) realloc(array->items, sizeof(u64) * new_capacity);
        if(new_items) {
            array->items = new_items;
            array->capacity = new_capacity;
        }
    }

    *item = popped_item;
    return 0;
}