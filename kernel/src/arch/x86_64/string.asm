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

; more optimized versions of libc string functions

global __fast_memcpy
align 16
__fast_memcpy:
    cmp rdx, 8
    jl .low

    mov rcx, rdx
    shr rcx, 3
    rep movsq

.low:
    mov rcx, rdx
    and rcx, 7
    rep movsb

    mov rax, rdi
    ret

global __fast_memset
align 16
__fast_memset:
    mov r8, rdi         ; dst

    and rsi, 0xFF
    mov rax, rsi        ; low 8 bits

    cmp rdx, 8
    jl .low

    shl rax, 8
    or rax, rsi         ; 16 bits
    shl rax, 8
    or rax, rsi         ; 24 bits
    shl rax, 8
    or rax, rsi         ; 32 bits

    mov r9, rax
    shl rax, 32
    or rax, r9          ; 64 bits

    mov rcx, rdx
    shr rcx, 3

    rep stosq

    and rdx, 7

.low:
    mov rcx, rdx
    rep stosb

    mov rax, r8         ; dst
    ret

global __fast_memmove_forward
align 16
__fast_memmove_forward:
    mov rcx, rdx
    rep movsb
    mov rax, rdi
    ret

global __fast_memmove_backward
align 16
__fast_memmove_backward:
    std
    add rdi, rdx
    add rsi, rdx

    dec rdi
    dec rsi

    mov rcx, rdx
    rep movsb
    cld
    mov rax, rdi
    inc rax
    ret
