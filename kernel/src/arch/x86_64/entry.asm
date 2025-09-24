[bits 64]

global _start
_start:
    ; this is a stub until the boot loader is functional
    cli
    hlt
    jmp _start
