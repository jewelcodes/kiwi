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

[bits 16]

%define BASE                    0x1000
%define CR3_PTR                 0x2000
%define STACK_PTR               0x2008
%define ENTRY_POINT_PTR         0x2010

section .rodata

global ap_early_main
ap_early_main:
    jmp 0x0000:.next + BASE - ap_early_main

.next:
    xor ax, ax
    mov ds, ax

    mov si, gdtr + BASE - ap_early_main
    lgdt [si]

    mov eax, [CR3_PTR]
    mov cr3, eax

    mov eax, 0x620
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or ah, 1
    wrmsr

    mov eax, cr0
    and eax, 0x9FFFFFFF
    or eax, 0x80000001
    mov cr0, eax

    jmp 0x08:.next2 + BASE - ap_early_main

[bits 64]

.next2:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rsp, [STACK_PTR]
    mov rbp, rsp
    mov rax, [ENTRY_POINT_PTR]
    cld
    call rax

.unreachable:
    cli
    hlt
    jmp .unreachable

align 16
gdt:
    ; 0x00 - null descriptor
gdt_null:
    dq 0

    ; 0x08 - 64-bit code descriptor
gdt_code64:
    .limit:         dw 0xFFFF
    .base_lo:       dw 0x0000
    .base_mi:       db 0x00
    .access:        db 0x9A     ; present, segment, executable, read access
    .flags:         db 0xAF     ; page granularity, 64-bit
    .base_hi:       db 0x00

    ; 0x10 - 64-bit data descriptor
gdt_data64:
    .limit:         dw 0xFFFF
    .base_lo:       dw 0x0000
    .base_mi:       db 0x00
    .access:        db 0x92     ; present, segment, non-executable, write access
    .flags:         db 0xAF     ; page granularity, 64-bit
    .base_hi:       db 0x00

gdt_end:

align 16
global gdtr
gdtr:
    .limit:         dw gdt_end - gdt - 1
    .base:          dd gdt + BASE - ap_early_main

global ap_early_main_end
ap_early_main_end: