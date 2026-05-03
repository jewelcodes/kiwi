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

#include <kiwi/debug.h>
#include <kiwi/arch/memmap.h>

struct StackFrame {
    u64 previous_rbp;
    u64 rip;
};

static inline int is_valid_stack_frame(u64 ptr) {
    return ptr && IS_KERNEL_ADDRESS(ptr) && (ptr % 8 == 0);
}

static inline const char *best_match(u64 addr, u64 *diff_out) {
    const char *name = NULL;
    u64 diff, best_diff = (u64) -1;
    for(usize i = 0; i < __ksyms_count; i++) {
        if(__ksyms[i].address <= addr) {
            diff = addr - __ksyms[i].address;
            if(diff < best_diff) {
                best_diff = diff;
                name = __ksyms[i].name;
            }
        } else {
            /* symbols are sorted by address so we can break early */
            break;
        }
    }
    if(name && diff_out)
        *diff_out = best_diff;
    return name;
}

void arch_call_trace(int level) {
    struct StackFrame *frame;
    const char *name;
    u64 rbp, offset;
    int frame_count = 0;

    debug_print(level, __FILE__+4, "call trace:");

    asm volatile("mov %%rbp, %0" : "=r" (rbp));

    while(is_valid_stack_frame(rbp)) {
        frame = (struct StackFrame *) rbp;
        if(!frame->rip || !IS_KERNEL_ADDRESS(frame->rip))
            break;
        frame_count++;

        name = best_match(frame->rip, &offset);
        if(name) {
            debug_print(level, NULL, "0x%016llX: "
                "<\e[97m%s\e[0m+0x%llX>",
                frame->rip, name, offset);
        } else
            debug_print(level, NULL, "0x%016llX", frame->rip);

        rbp = frame->previous_rbp;
    }

    if(!frame_count)
        debug_print(level, NULL, " <no valid stack frames found>");
}