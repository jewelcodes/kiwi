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

#define PAGE_SIZE                   4096
#define PAGE_MASK                   ((PAGE_SIZE - 1) | 0xF000000000000000ULL)
#define PAGE_ALIGN_UP(x)            (((x) + PAGE_MASK) & ~PAGE_MASK)
#define PAGE_ALIGN_DOWN(x)          ((x) & ~PAGE_MASK)
#define PAGE_ALIGNED(x)             (((x) & PAGE_MASK) == 0)

#define ARCH_MMIO_BASE              0xFFFFA00000000000ULL
#define ARCH_HHDM_BASE              0xFFFFB00000000000ULL
#define ARCH_VMM_BASE               0xFFFF800000000000ULL
#define ARCH_KERNEL_HEAP_BASE       0xFFFF900000000000ULL
#define ARCH_KERNEL_IMAGE_BASE      0xFFFFFFFF80100000ULL
