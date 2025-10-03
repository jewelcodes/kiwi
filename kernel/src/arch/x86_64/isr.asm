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

%include "src/arch/x86_64/stack.inc"

extern arch_exception_handler

; exception_without_code name, number
%macro exception_without_code 2
global %1
align 16
%1:
    cli
    cld
    push qword 0
    pushaq
    mov rdi, %2
    xor rsi, rsi
    mov rdx, rsp
    call arch_exception_handler
    popaq
    add rsp, 8
    iretq
%endmacro

; exception_with_code name, number
%macro exception_with_code 2
global %1
align 16
%1:
    cli
    cld
    pushaq
    mov rdi, %2
    mov rsi, [rsp+120]
    mov rdx, rsp
    call arch_exception_handler
    popaq
    add rsp, 8
    iretq
%endmacro

exception_without_code isr0_handler, 0
exception_without_code isr1_handler, 1
exception_without_code isr2_handler, 2
exception_without_code isr3_handler, 3
exception_without_code isr4_handler, 4
exception_without_code isr5_handler, 5
exception_without_code isr6_handler, 6
exception_without_code isr7_handler, 7
exception_with_code    isr8_handler, 8
exception_without_code isr9_handler, 9
exception_with_code    isr10_handler, 10
exception_with_code    isr11_handler, 11
exception_with_code    isr12_handler, 12
exception_with_code    isr13_handler, 13
exception_with_code    isr14_handler, 14
exception_without_code isr15_handler, 15
exception_without_code isr16_handler, 16
exception_with_code    isr17_handler, 17
exception_without_code isr18_handler, 18
exception_without_code isr19_handler, 19
exception_without_code isr20_handler, 20
exception_with_code    isr21_handler, 21
exception_without_code isr22_handler, 22
exception_without_code isr23_handler, 23
exception_without_code isr24_handler, 24
exception_without_code isr25_handler, 25
exception_without_code isr26_handler, 26
exception_without_code isr27_handler, 27
exception_without_code isr28_handler, 28
exception_with_code    isr29_handler, 29
exception_with_code    isr30_handler, 30
exception_without_code isr31_handler, 31
