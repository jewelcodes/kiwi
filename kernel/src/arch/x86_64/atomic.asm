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

; void arch_spin_backoff(void);
; int arch_cas32(u32 *ptr, u32 old, u32 new);
; int arch_cas64(u64 *ptr, u64 old, u64 new);
; u32 arch_spinlock_acquire(lock_t *lock);
; void arch_spinlock_release(lock_t *lock);

global arch_spin_backoff
align 16
arch_spin_backoff:
    pause
    ret

global arch_cas32
align 16
arch_cas32:
    mov eax, esi        ; old
    mov edx, edi        ; new
    lock cmpxchg [rdi], edx
    sete al
    movzx rax, al
    ret

global arch_cas64
align 16
arch_cas64:
    mov rax, rsi
    mov rdx, rdi
    lock cmpxchg [rdi], rdx
    sete al
    movzx rax, al
    ret

global arch_spinlock_acquire
align 16
arch_spinlock_acquire:
    lock bts qword [rdi], 0
    jnc .done
    pause
    jmp arch_spinlock_acquire

.done:
    mov eax, 1
    ret

global arch_spinlock_release
align 16
arch_spinlock_release:
    lock btr qword [rdi], 0
    ret
