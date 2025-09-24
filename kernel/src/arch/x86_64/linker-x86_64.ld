OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS
{
    . = 0xFFFFFFFF80000000;

    .text BLOCK(4096) : ALIGN(4096)
    {
        *(.text)
    }

    .rodata BLOCK(4096) : ALIGN(4096)
    {
        *(.rodata)
    }

    .data BLOCK(4096) : ALIGN(4096)
    {
        *(.data)
    }

    .bss BLOCK(4096) : ALIGN(4096)
    {
        *(.bss)
    }
}
