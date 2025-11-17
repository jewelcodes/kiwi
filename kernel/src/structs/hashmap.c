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

#include <kiwi/structs/hashmap.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_BUCKET_COUNT            16
#define GROWTH_LOAD_FACTOR              75 /* percent */
#define SHRINK_LOAD_FACTOR              25 /* percent */

static inline u64 rotl64(u64 x, u8 r) {
    return (x << r) | (x >> (64 - r));
}

static u64 xxhash64_string(const char *str) {
    usize len = strlen(str);
    const u64 prime1 = 11400714785074694791ULL;
    const u64 prime2 = 14029467366897019727ULL;
    const u64 prime3 = 1609587929392839161ULL;
    const u64 prime4 = 9650029242287828579ULL;
    const u64 prime5 = 2870177450012600261ULL;
    const u64 seed = 0x9E3779B185EBCA87ULL;

    const u8 *p = (const u8 *) str;
    const u8 *const end = p + len;
    u64 hash;

    if (len >= 32) {
        u64 v1 = seed + prime1 + prime2;
        u64 v2 = seed + prime2;
        u64 v3 = seed + 0;
        u64 v4 = seed - prime1;

        const u8 *limit = end - 32;
        do {
            v1 += (*(u64*)p)  *prime2;
            v1 = rotl64(v1, 31);
            v1 *= prime1;
            p += 8;

            v2 += (*(u64*)p)  *prime2;
            v2 = rotl64(v2, 31);
            v2 *= prime1;
            p += 8;

            v3 += (*(u64*)p)  *prime2;
            v3 = rotl64(v3, 31);
            v3 *= prime1;
            p += 8;

            v4 += (*(u64*)p)  *prime2;
            v4 = rotl64(v4, 31);
            v4 *= prime1;
            p += 8;
        } while (p <= limit);

        hash = rotl64(v1, 1) + rotl64(v2, 7) + rotl64(v3, 12) + rotl64(v4, 18);
    } else {
        hash = seed + prime5;
    }

    while (p + 8 <= end) {
        u64 k1 = *(u64 *)p;
        k1 *= prime2; k1 = rotl64(k1, 31); k1 *= prime1;
        hash ^= k1;
        hash = rotl64(hash, 27)  *prime1 + prime4;
        p += 8;
    }

    if (p + 4 <= end) {
        hash ^= (*(u32 *)p) * prime1;
        hash = rotl64(hash, 23) * prime2 + prime3;
        p += 4;
    }

    while (p < end) {
        hash ^= (*p) * prime5;
        hash = rotl64(hash, 11)  *prime1;
        p++;
    }

    hash ^= hash >> 33;
    hash *= prime2;
    hash ^= hash >> 29;
    hash *= prime3;
    hash ^= hash >> 32;

    return hash;
}

static int hashmap_resize(Hashmap *map, u64 new_bucket_count) {
    HashmapEntry **new_buckets = (HashmapEntry **) calloc(new_bucket_count, sizeof(HashmapEntry *));
    if(!new_buckets) {
        return -1;
    }

    for(u64 i = 0; i < map->bucket_count; i++) {
        HashmapEntry *entry = map->buckets[i];
        while(entry) {
            HashmapEntry *next = entry->next;
            u64 new_index = entry->key % new_bucket_count;
            entry->next = new_buckets[new_index];
            new_buckets[new_index] = entry;
            entry = next;
        }
    }

    free(map->buckets);
    map->buckets = new_buckets;
    map->bucket_count = new_bucket_count;

    return 0;
}

Hashmap *hashmap_create(void) {
    Hashmap *map = (Hashmap *)malloc(sizeof(Hashmap));
    if(!map) {
        return NULL;
    }

    map->bucket_count = INITIAL_BUCKET_COUNT;
    map->count = 0;
    map->buckets = (HashmapEntry **) calloc(map->bucket_count, sizeof(HashmapEntry *));
    if(!map->buckets) {
        free(map);
        return NULL;
    }

    return map;
}

void hashmap_destroy(Hashmap *map) {
    if(!map) {
        return;
    }

    if(!map->buckets) {
        free(map);
        return;
    }

    for(u64 i = 0; i < map->bucket_count; i++) {
        HashmapEntry *entry = map->buckets[i];
        while(entry) {
            HashmapEntry *next = entry->next;
            free(entry);
            entry = next;
        }
    }

    free(map->buckets);
    free(map);
}

int hashmap_put(Hashmap *map, u64 key, u64 value) {
    if(!map) {
        return -1;
    }

    u64 load_factor = (map->count * 100) / map->bucket_count;
    if(load_factor >= GROWTH_LOAD_FACTOR) {
        if(hashmap_resize(map, map->bucket_count * 2) != 0) {
            return -1;
        }
    }

    u64 index = key % map->bucket_count;
    HashmapEntry *entry = map->buckets[index];
    while(entry) {
        if(entry->key == key) {
            entry->value = value;
            return 0;
        }
        entry = entry->next;
    }

    HashmapEntry *new_entry = (HashmapEntry *) malloc(sizeof(HashmapEntry));
    if(!new_entry) {
        return -1;
    }

    new_entry->key = key;
    new_entry->value = value;
    new_entry->next = map->buckets[index];
    map->buckets[index] = new_entry;
    map->count++;

    return 0;
}

int hashmap_get(Hashmap *map, u64 key, u64 *value) {
    if(!map || !value) {
        return -1;
    }

    u64 index = key % map->bucket_count;
    HashmapEntry *entry = map->buckets[index];
    while(entry) {
        if(entry->key == key) {
            *value = entry->value;
            return 0;
        }
        entry = entry->next;
    }

    return -1;
}

int hashmap_remove(Hashmap *map, u64 key) {
    if(!map) {
        return -1;
    }

    u64 index = key % map->bucket_count;
    HashmapEntry *entry = map->buckets[index];
    HashmapEntry *prev = NULL;
    while(entry) {
        if(entry->key == key) {
            if(prev) {
                prev->next = entry->next;
            } else {
                map->buckets[index] = entry->next;
            }
            free(entry);
            map->count--;

            u64 load_factor = (map->count * 100) / map->bucket_count;
            if((load_factor <= SHRINK_LOAD_FACTOR) && (map->bucket_count > INITIAL_BUCKET_COUNT)) {
                hashmap_resize(map, map->bucket_count / 2);
            }

            return 0;
        }
        prev = entry;
        entry = entry->next;
    }

    return -1;
}

int hashmap_contains(Hashmap *map, u64 key) {
    if(!map) {
        return 0;
    }

    u64 index = key % map->bucket_count;
    HashmapEntry *entry = map->buckets[index];
    while(entry) {
        if(entry->key == key) {
            return 1;
        }
        entry = entry->next;
    }

    return 0;
}

int hashmap_put_string(Hashmap *map, const char *key, u64 value) {
    if(!map || !key || !*key) {
        return -1;
    }

    u64 hash = xxhash64_string(key) + strlen(key);
    return hashmap_put(map, hash, value);
}

int hashmap_get_string(Hashmap *map, const char *key, u64 *value) {
    if(!map || !key || !*key || !value) {
        return -1;
    }

    u64 hash = xxhash64_string(key) + strlen(key);
    return hashmap_get(map, hash, value);
}
