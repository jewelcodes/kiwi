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
    mov sp, STACK_OFFSET

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
    ; now we can switch to protected mode
    cli
    lgdt [gdtr]

    mov eax, cr0
    or al, 1
    mov cr0, eax

    jmp CODE32_SEGMENT:.next

[bits 32]

.next:
    mov ax, DATA32_SEGMENT
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    movzx esp, sp
    add esp, STACK_SEGMENT << 4
    mov ebp, esp

    ; enable global caching
    mov eax, cr0
    and eax, 0xBFFFFFFF
    mov cr0, eax

    extern main
    call main

.halt:
    cli
    hlt
    jmp .halt ; this is unreachable

[bits 16]

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


section .stubdata

no64_msg:                   db "A 64-bit CPU is required to run kiwi", 0
no_memory_msg:              db "At least 16 MB of RAM is required to run kiwi", 0

align 16
gdt:
    ; 0x00 - null descriptor

gdt_null:
    dq 0

    ; 0x08 - 32-bit code descriptor
gdt_code32:
    .limit:         dw 0xFFFF
    .base_lo:       dw 0x0000
    .base_mi:       db 0x00
    .access:        db 0x9A     ; present, segment, executable, read access
    .flags:         db 0xCF     ; page granularity, 32-bit
    .base_hi:       db 0x00

    ; 0x10 - 32-bit data descriptor
gdt_data32:
    .limit:         dw 0xFFFF
    .base_lo:       dw 0x0000
    .base_mi:       db 0x00
    .access:        db 0x92     ; present, segment, non-executable, write access
    .flags:         db 0xCF     ; page granularity, 32-bit
    .base_hi:       db 0x00

    ; 0x18 - 16-bit code descriptor
gdt_code16:
    .limit:         dw 0xFFFF
    .base_lo:       dw 0x0000
    .base_mi:       db 0x00
    .access:        db 0x9A     ; present, segment, executable, read access
    .flags:         db 0x0F     ; byte granularity, 16-bit
    .base_hi:       db 0x00

    ; 0x20 - 16-bit data descriptor
gdt_data16:
    .limit:         dw 0xFFFF
    .base_lo:       dw 0x0000
    .base_mi:       db 0x00
    .access:        db 0x92     ; present, segment, non-executable, write access
    .flags:         db 0x0F     ; byte granularity, 16-bit
    .base_hi:       db 0x00

    ; 0x28 - 64-bit code descriptor
gdt_code64:
    .limit:         dw 0xFFFF
    .base_lo:       dw 0x0000
    .base_mi:       db 0x00
    .access:        db 0x9A     ; present, segment, executable, read access
    .flags:         db 0xAF     ; page granularity, 64-bit
    .base_hi:       db 0x00

    ; 0x30 - 64-bit data descriptor
gdt_data64:
    .limit:         dw 0xFFFF
    .base_lo:       dw 0x0000
    .base_mi:       db 0x00
    .access:        db 0x92     ; present, segment, non-executable, write access
    .flags:         db 0xAF     ; page granularity, 64-bit
    .base_hi:       db 0x00

gdt_end:

align 16
gdtr:
    .limit:         dw gdt_end - gdt - 1
    .base:          dd gdt

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

