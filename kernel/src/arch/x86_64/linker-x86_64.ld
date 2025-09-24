OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS
{
    . = 0xFFFFFFFF80100000;
    .text : AT(0x00100000)
    {
        *(.text .text.*)
    }

    . = ALIGN(4096);
    .rodata : AT(ADDR(.rodata) - 0xFFFFFFFF80100000 + 0x00100000)
    {
        *(.rodata .rodata.*)
    }

    . = ALIGN(4096);
    .data : AT(ADDR(.data) - 0xFFFFFFFF80100000 + 0x00100000)
    {
        *(.data .data.*)
    }

    . = ALIGN(4096);
    .bss : AT(ADDR(.bss) - 0xFFFFFFFF80100000 + 0x00100000)
    {
        *(COMMON)
        *(.bss .bss.*)
    }
}
