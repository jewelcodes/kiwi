;
; kiwi - general-purpose high-performance operating system
;
; Copyright (c) 2025 Omar Elghoul
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in all
; copies or substantial portions of the Software.
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
; SOFTWARE.
;

[bits 64]

section .text

%define OFFSET_R15              0
%define OFFSET_R14              8
%define OFFSET_R13              16
%define OFFSET_R12              24
%define OFFSET_R11              32
%define OFFSET_R10              40
%define OFFSET_R9               48
%define OFFSET_R8               56
%define OFFSET_RBP              64
%define OFFSET_RDI              72
%define OFFSET_RSI              80
%define OFFSET_RDX              88
%define OFFSET_RCX              96
%define OFFSET_RBX              104
%define OFFSET_RAX              112
%define OFFSET_RIP              120
%define OFFSET_CS               128
%define OFFSET_RFLAGS           136
%define OFFSET_RSP              144
%define OFFSET_SS               152

; void arch_switch_context(MachineContext *context, uptr page_tables)

global arch_switch_context
align 16
arch_switch_context:
    mov rax, cr3
    cmp rax, rsi
    jz .switched_page_tables

    mov cr3, rsi

.switched_page_tables:
    mov rax, [rdi + OFFSET_SS]
    push rax
    mov rax, [rdi + OFFSET_RSP]
    push rax
    mov rax, [rdi + OFFSET_RFLAGS]
    or ax, 0x202               ; IRQs
    push rax
    mov rax, [rdi + OFFSET_CS]
    push rax
    and al, 3
    jz .finish_stack

.load_user_segments:
    mov rax, [rdi + OFFSET_SS]
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    swapgs

.finish_stack:
    mov rax, [rdi + OFFSET_RIP]
    push rax

    mov rax, [rdi + OFFSET_RAX]
    mov rbx, [rdi + OFFSET_RBX]
    mov rcx, [rdi + OFFSET_RCX]
    mov rdx, [rdi + OFFSET_RDX]
    mov rsi, [rdi + OFFSET_RSI]
    mov rbp, [rdi + OFFSET_RBP]
    mov r8,  [rdi + OFFSET_R8]
    mov r9,  [rdi + OFFSET_R9]
    mov r10, [rdi + OFFSET_R10]
    mov r11, [rdi + OFFSET_R11]
    mov r12, [rdi + OFFSET_R12]
    mov r13, [rdi + OFFSET_R13]
    mov r14, [rdi + OFFSET_R14]
    mov r15, [rdi + OFFSET_R15]
    mov rdi, [rdi + OFFSET_RDI]

    iretq