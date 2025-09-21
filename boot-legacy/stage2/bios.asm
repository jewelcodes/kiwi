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

[bits 32]

section .text

; bios_int():
; @brief: invokes a BIOS interrupt in real mode from protected mode
; @input: u8 int_no - the interrupt number
;         Registers* regs - input register states and output register buffer
; @output: Registers* - same pointer as input with updated register states

global bios_int
bios_int:
    ; these regs must be preserved by the callee
    push ebx
    push esi
    push edi
    push ebp

    mov eax, [esp + 20]
    mov byte [.interrupt + 1], al
    and esp, 0xFFFF
    jmp CODE16_SEGMENT:.next

[bits 16]

.next:
    mov eax, cr0
    and al, 0xFE
    mov cr0, eax

    jmp 0x0000:.next2

.next2:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ax, STACK_SEGMENT
    mov ss, ax

    sti

    mov bx, sp
    mov esi, [ss:bx + 24]
    mov eax, esi
    shr eax, 4
    mov [.regs_segment], ax
    and esi, 0x0F
    mov [.regs_offset], si
    mov ds, ax

    mov ax, [esi + 36] ; flags
    push ax
    popf

    mov ax, [esi + 32] ; es
    mov es, ax

    mov ax, [esi + 28] ; ds
    push ax ; we still need our ds calculated for now

    mov eax, [esi + 0]
    mov ebx, [esi + 4]
    mov ecx, [esi + 8]
    mov edx, [esi + 12]
    mov edi, [esi + 20]
    mov ebp, [esi + 24]

    mov esi, [esi + 16]
    pop ds

    sti

.interrupt:
    db 0xCD, 0x00 ; second byte is the interrupt number

    pushf ; push before xor because we will clobber flags

    xor bp, bp
    mov ds, bp
    mov bp, [.regs_segment]
    mov si, [.regs_offset]
    mov ds, bp

    mov [si + 0], eax
    mov [si + 4], ebx
    mov [si + 8], ecx
    mov [si + 12], edx
    mov [si + 20], edi
    mov [si + 24], ebp

    pop ax  ; flags
    and eax, 0xFFFF
    mov [si + 36], ax

    ; jump back to protected mode
    extern gdtr
    xor ax, ax
    mov ds, ax
    lgdt [gdtr]

    cli
    mov eax, cr0
    or al, 1
    mov cr0, eax

    jmp CODE32_SEGMENT:.next3

[bits 32]

.next3:
    mov ax, DATA32_SEGMENT
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    movzx esp, sp
    add esp, STACK_SEGMENT << 4

    pop ebp
    pop edi
    pop esi
    pop ebx

    cld

    mov eax, [esp + 8] ; return the same pointer passed in
    ret

    .regs_segment:              resb 2
    .regs_offset:               resb 2
