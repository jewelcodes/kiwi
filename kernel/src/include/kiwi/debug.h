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

#define DEBUG_LEVEL_INFO    1
#define DEBUG_LEVEL_WARN    2
#define DEBUG_LEVEL_ERROR   3
#define DEBUG_LEVEL_PANIC   4

#define debug_info(fmt, ...)    debug_print(DEBUG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define debug_warn(fmt, ...)    debug_print(DEBUG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#define debug_error(fmt, ...)   debug_print(DEBUG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define debug_panic(fmt, ...)   debug_print(DEBUG_LEVEL_PANIC, fmt, ##__VA_ARGS__)

void debug_print(int level, const char *fmt, ...);
