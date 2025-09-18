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

    %include                "stage2/memory.inc"

[bits 16]

section .stub
global _start
_start:
    cli
    cld

    xor ax, ax
    mov es, ax
    mov di, boot_partition
    mov cx, 16 / 2
    rep movsw

    mov ds, ax
    mov [bios_boot_info.bootdisk], dl

    mov ax, STACK_SEGMENT
    mov ss, ax
    xor sp, sp

    ; ensure we have a 64-bit cpu
    mov eax, 0x80000001
    cpuid
    and edx, 0x20000000
    jz error.no64

    ; and >16 MB RAM
    clc
    xor cx, cx
    xor dx, dx
    mov ax, 0xE801
    int 0x15
    jc error.no_memory

    jcxz .check_memory

    mov ax, cx
    mov bx, dx

.check_memory:
    and bx, bx          ; bx = pages >16 MB
    jz error.no_memory

    ; enable A20 gate
    clc
    mov ax, 0x2402      ; query
    int 0x15
    jc .try_fast_a20

    and ah, ah
    jnz .try_fast_a20

    cmp al, 1
    jz .a20_done

    ; try to enable using BIOS
    clc
    mov ax, 0x2401
    int 0x15
    jc .try_fast_a20

    and ah, ah
    jz .a20_done

.try_fast_a20:
    in al, 0x92
    test al, 2
    jnz .a20_done

    or al, 2
    out 0x92, al
    nop

.a20_done:
    mov ah, 0x0E
    mov al, 'A' 
    int 0x10

    cli
    hlt

error:

.no64:
    mov si, no64_msg
    jmp .print

.no_memory:
    mov si, no_memory_msg

.print:
    lodsb
    and al, al
    jz .halt
    mov ah, 0x0E
    int 0x10
    jmp .print

.halt:
    sti
    hlt
    jmp .halt

no64_msg:                   db "64-bit CPU is required to run kiwi", 0

no_memory_msg:              db "At least 16 MB of RAM is required to run kiwi", 0

; this structure will be passed to the main C program

global bios_boot_info
bios_boot_info:
    .bootdisk:              db 0

    boot_partition:
        .bootable:          db 0
        .start_chs:         times 3 db 0
        .type:              db 0
        .end_chs:           times 3 db 0
        .start_lba:         dd 0
        .size_in_sectors:   dd 0

