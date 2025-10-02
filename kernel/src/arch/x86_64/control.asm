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

; u64 arch_get_cr0(void);
; u64 arch_get_cr2(void);
; u64 arch_get_cr3(void);
; u64 arch_get_cr4(void);
; void arch_set_cr0(u64 value);
; void arch_set_cr3(u64 value);
; void arch_set_cr4(u64 value);

global arch_get_cr0
align 16
arch_get_cr0:
    mov rax, cr0
    ret

global arch_get_cr2
align 16
arch_get_cr2:
    mov rax, cr2
    ret

global arch_get_cr3
align 16
arch_get_cr3:
    mov rax, cr3
    ret

global arch_get_cr4
align 16
arch_get_cr4:
    mov rax, cr4
    ret

global arch_set_cr0
align 16
arch_set_cr0:
    mov cr0, rdi
    ret

global arch_set_cr3
align 16
arch_set_cr3:
    mov cr3, rdi
    ret

global arch_set_cr4
align 16
arch_set_cr4:
    mov cr4, rdi
    ret
