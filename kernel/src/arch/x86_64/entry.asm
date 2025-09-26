[bits 64]

section .text

global _start
align 16
_start:
    cld

    mov rsp, early_boot_stack_top
    mov rbp, rsp

    mov rdi, rax
    extern arch_early_main
    call arch_early_main

.unreachable:
    cli
    hlt
    jmp .unreachable

section .bss

    align 16
    early_boot_stack_bottom:           resb 32768
    early_boot_stack_top:
