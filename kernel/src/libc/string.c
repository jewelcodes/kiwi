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

#include <kiwi/types.h>
#include <string.h>
#include <stdlib.h>

void *__fast_memcpy(void *dst, const void *src, size_t n);
void *__fast_memmove_forward(void *dst, const void *src, size_t n);
void *__fast_memmove_backward(void *dst, const void *src, size_t n);
void *__fast_memset(void *s, int c, size_t n);

void *memcpy(void *dst, const void *src, size_t n) {
    if(n == 0 || dst == src) {
        return dst;
    }

    uptr dst_start = (uptr) dst;
    uptr dst_end = dst_start + n;
    uptr src_start = (uptr) src;
    uptr src_end = src_start + n;

    if((dst_start < src_end) || (src_start < dst_end)) {
        usize overlap_size = (dst_end < src_end)
            ? (dst_end - src_start)
            : (src_end - dst_start);

        if(overlap_size < 8) {
            return memmove(dst, src, n);
        }
    }

    return __fast_memcpy(dst, src, n);
}

void *memmove(void *dst, const void *src, size_t n) {
    if(!n || dst == src) {
        return dst;
    }

    uptr dst_start = (uptr) dst;
    uptr dst_end = dst_start + n;
    uptr src_start = (uptr) src;
    uptr src_end = src_start + n;

    if((dst_start < src_end) || (src_start < dst_end)) {
        usize overlap_size = (dst_end < src_end)
            ? (dst_end - src_start)
            : (src_end - dst_start);

        if(overlap_size >= 8) {
            return __fast_memcpy(dst, src, n);
        }
    }

    if(dst_start < src_start) {
        return __fast_memmove_forward(dst, src, n);
    }

    return __fast_memmove_backward(dst, src, n);
}

size_t strlen(const char *str) {
    size_t len = 0;
    while(str[len]) len++;
    return len;
}

char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while(*src) {
        *d++ = *src++;
    }
    *d = '\0';
    return dst;
}

void *memset(void *s, int c, size_t n) {
    return __fast_memset(s, c, n);
}

int memcmp(const void *s1, const void *s2, size_t n) {
    char *p1 = (char *) s1;
    char *p2 = (char *) s2;
    for(size_t i = 0; i < n; i++) {
        if(p1[i] != p2[i]) {
            return (unsigned char)p1[i] - (unsigned char)p2[i];
        }
    }
    return 0;
}

int strcmp(const char *s1, const char *s2) {
    while(*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

char *strdup(const char *s) {
    size_t len = strlen(s);
    char *dup = malloc(len + 1);
    if(!dup) {
        return NULL;
    }
    strcpy(dup, s);
    return dup;
}
