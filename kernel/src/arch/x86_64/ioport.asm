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

; u8 arch_inport8(u16 port);
; void arch_outport8(u16 port, u8 data);
; u16 arch_inport16(u16 port);
; void arch_outport16(u16 port, u16 data);
; u32 arch_inport32(u16 port);
; void arch_outport32(u16 port, u32 data);

global arch_inport8
align 16
arch_inport8:
    mov dx, di
    in al, dx
    ret

global arch_outport8
align 16
arch_outport8:
    mov dx, di
    mov al, sil
    out dx, al
    ret

global arch_inport16
align 16
arch_inport16:
    mov dx, di
    in ax, dx
    ret

global arch_outport16
align 16
arch_outport16:
    mov dx, di
    mov ax, si
    out dx, ax
    ret

global arch_inport32
align 16
arch_inport32:
    mov dx, di
    in eax, dx
    ret

global arch_outport32
align 16
arch_outport32:
    mov dx, di
    mov eax, esi
    out dx, eax
    ret
