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

typedef struct HashmapEntry {
    u64 key;
    u64 value;
    struct HashmapEntry *next;
} HashmapEntry;

typedef struct Hashmap {
    HashmapEntry **buckets;
    u64 bucket_count;
    u64 count;
} Hashmap;

Hashmap *hashmap_create(void);
void hashmap_destroy(Hashmap *map);
int hashmap_put(Hashmap *map, u64 key, u64 value);
int hashmap_get(Hashmap *map, u64 key, u64 *value);
int hashmap_remove(Hashmap *map, u64 key);
int hashmap_contains(Hashmap *map, u64 key);
int hashmap_put_string(Hashmap *map, const char *key, u64 value);
int hashmap_get_string(Hashmap *map, const char *key, u64 *value);
