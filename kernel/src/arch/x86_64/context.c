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

#include <kiwi/arch/context.h>
#include <kiwi/debug.h>
#include <string.h>
#include <stdlib.h>

/* TODO: obviously don't hardcode this */
#define KERNEL_STACK_SIZE       65536

MachineContext *arch_set_context(MachineContext *dst, const MachineContext *src) {
    return memcpy(dst, src, sizeof(MachineContext));
}

void arch_dump_regs(int level, const RegisterState *regs) {
    debug_print(level, NULL, "cs:rip = 0x%02llX:0x%016llX, ss:rsp = 0x%02llX:0x%016llX",
        regs->cs, regs->rip, regs->ss, regs->rsp);
    debug_print(level, NULL, "rflags: 0x%016llX", regs->rflags);
    debug_print(level, NULL, "rax: 0x%016llX rbx: 0x%016llX rcx: 0x%016llX", 
        regs->rax, regs->rbx, regs->rcx);
    debug_print(level, NULL, "rdx: 0x%016llX rsi: 0x%016llX rdi: 0x%016llX",
        regs->rdx, regs->rsi, regs->rdi);
    debug_print(level, NULL, "rbp: 0x%016llX r8:  0x%016llX r9:  0x%016llX",
        regs->rbp, regs->r8, regs->r9);
    debug_print(level, NULL, "r10: 0x%016llX r11: 0x%016llX r12: 0x%016llX",
        regs->r10, regs->r11, regs->r12);
    debug_print(level, NULL, "r13: 0x%016llX r14: 0x%016llX r15: 0x%016llX",
        regs->r13, regs->r14, regs->r15);
    debug_print(level, NULL, "cr0: 0x%016llX cr2: 0x%016llX cr3: 0x%016llX",
        arch_get_cr0(), arch_get_cr2(), arch_get_cr3());
}

int arch_create_kernel_context(MachineContext *ctx_dst,
                               void (*entry_point)(void *), void *arg) {
    void *stack = malloc(KERNEL_STACK_SIZE);
    if(!stack)
        return -1;

    memset(ctx_dst, 0, sizeof(MachineContext));
    ctx_dst->regs.rip = (u64) entry_point;
    ctx_dst->regs.rdi = (u64) arg;
    ctx_dst->regs.rsp = (u64) stack + KERNEL_STACK_SIZE;
    ctx_dst->regs.cs = GDT_KERNEL_CODE << 3;
    ctx_dst->regs.ss = GDT_KERNEL_DATA << 3;
    ctx_dst->regs.rflags = 0x202;   /* interrupts enabled */
    return 0;
}
