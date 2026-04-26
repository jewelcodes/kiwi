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

#include <kiwi/arch/context.h>
#include <kiwi/arch/x86_64.h>
#include <kiwi/debug.h>
#include <stdlib.h>
#include <string.h>

/* TODO: don't hard code these */
#define KERNEL_STACK_SIZE           32768
#define USER_STACK_SIZE             65536

MachineContext *arch_create_context(int user, void (*start)(void *), void *arg,
                                    uptr *kernel_stack, uptr *user_stack,
                                    uptr *page_tables_ptr) {
    MachineContext *context = calloc(1, sizeof(MachineContext));
    if(!context) {
        return NULL;
    }

    uptr kernel_rsp = (uptr) calloc(1, KERNEL_STACK_SIZE);
    if(!kernel_rsp) {
fail_1:
        free(context);
        return NULL;
    }

    uptr user_rsp = (uptr) calloc(1, USER_STACK_SIZE);
    if(!user_rsp) {
fail_2:
        free((void *) kernel_rsp);
        goto fail_1;
    }

    if(page_tables_ptr) {
        uptr page_tables = arch_new_page_tables();
        if(!page_tables) {
            free((void *) user_rsp);
            goto fail_2;
        }

        *page_tables_ptr = page_tables;
    }

    if(user) {
        context->cs = (GDT_USER_CODE << 3) | 0x03;
        context->ss = (GDT_USER_DATA << 3) | 0x03;
        context->rsp = user_rsp + USER_STACK_SIZE;
    } else {
        context->cs = GDT_KERNEL_CODE << 3;
        context->ss = GDT_KERNEL_DATA << 3;
        context->rsp = kernel_rsp + KERNEL_STACK_SIZE;
    }

    context->rip = (u64) start;
    context->rdi = (u64) arg;
    context->rflags = 0x202;  // interrupts, parity
    *kernel_stack = kernel_rsp + KERNEL_STACK_SIZE;
    *user_stack = user_rsp + USER_STACK_SIZE;
    return context;
}

MachineContext *arch_save_context(MachineContext *dst, const MachineContext *src) {
    return memcpy(dst, src, sizeof(MachineContext));
}

void arch_dump_context(int level, const MachineContext *context) {
    debug_print(level, NULL, "cs:rip = 0x%02llX:0x%016llX, ss:rsp = 0x%02llX:0x%016llX",
        context->cs, context->rip, context->ss, context->rsp);
    debug_print(level, NULL, "rflags: 0x%016llX", context->rflags);
    debug_print(level, NULL, "rax: 0x%016llX rbx: 0x%016llX rcx: 0x%016llX", 
        context->rax, context->rbx, context->rcx);
    debug_print(level, NULL, "rdx: 0x%016llX rsi: 0x%016llX rdi: 0x%016llX",
        context->rdx, context->rsi, context->rdi);
    debug_print(level, NULL, "rbp: 0x%016llX r8:  0x%016llX r9:  0x%016llX",
        context->rbp, context->r8, context->r9);
    debug_print(level, NULL, "r10: 0x%016llX r11: 0x%016llX r12: 0x%016llX",
        context->r10, context->r11, context->r12);
    debug_print(level, NULL, "r13: 0x%016llX r14: 0x%016llX r15: 0x%016llX",
        context->r13, context->r14, context->r15);
    debug_print(level, NULL, "cr0: 0x%016llX cr2: 0x%016llX cr3: 0x%016llX",
        arch_get_cr0(), arch_get_cr2(), arch_get_cr3());
}