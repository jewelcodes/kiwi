OUTPUT_FORMAT(binary)
ENTRY(_start)

SECTIONS
{
    . = 0x500;

    .stub BLOCK(8) : ALIGN(8)
    {
        *(.stub)
    }

    .text BLOCK(8) : ALIGN(8)
    {
        *(.text)
    }

    .rodata BLOCK(8) : ALIGN(8)
    {
        *(.rodata)
    }

    .data BLOCK(8) : ALIGN(8)
    {
        *(.data)
    }

    .bss BLOCK(8) : ALIGN(8)
    {
        *(.bss)
    }
}