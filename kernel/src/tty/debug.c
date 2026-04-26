/*
 * kiwi - general-purpose high-performance operating system
 * 
 * Copyright (c) 2025-26 Omar Elghoul
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

#include <kiwi/debug.h>
#include <kiwi/timer.h>
#include <kiwi/arch/atomic.h>
#include <stdio.h>

static lock_t debug_lock = LOCK_INITIAL;

int debug_level = DEBUG_LEVEL_INFO;

void debug_print(int level, const char *file, const char *fmt, ...) {
    va_list args;

    if(level == DEBUG_LEVEL_PANIC)
        debug_level = DEBUG_LEVEL_PANIC;
    if(level < debug_level)
        return;

    arch_spinlock_acquire(&debug_lock);

    u64 ticks = uptime_ns();
    printf("\e[35m[%4llu.%06llu] ", ticks / SECOND, ticks % SECOND / MICROSECOND);

    if(file) {
        switch(level) {
        case DEBUG_LEVEL_ERROR:
        case DEBUG_LEVEL_PANIC:
            printf("\e[91m"); // bright red
            break;
        case DEBUG_LEVEL_WARN:
            printf("\e[93m"); // bright yellow
            break;
        case DEBUG_LEVEL_INFO:
        default:
            printf("\e[32m"); // green
        }

        printf("%s: \e[0m", file);
    } else {
        printf("\e[0m "); // reset color
    }

    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
    arch_spinlock_release(&debug_lock);

    if(level == DEBUG_LEVEL_PANIC) {
        for(;;);
    }
}
