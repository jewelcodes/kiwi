/*
 * pulse - a highly scalable SSD-first file system with predictable logarithmic
 * bounds across all operations
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

#include <pulse/pulse.h>
#include <string.h>

static inline u64 rotl64(u64 x, u8 r) {
    return (x << r) | (x >> (64 - r));
}

u64 xxhash64(const void *data, usize len) {
    const u64 prime1 = 11400714785074694791ULL;
    const u64 prime2 = 14029467366897019727ULL;
    const u64 prime3 = 1609587929392839161ULL;
    const u64 prime4 = 9650029242287828579ULL;
    const u64 prime5 = 2870177450012600261ULL;
    const u64 seed = 0x9E3779B185EBCA87ULL;

    const u8 *p = (const u8 *) data;
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

    hash += len;

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
