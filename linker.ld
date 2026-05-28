ENTRY(_start)
OUTPUT_FORMAT("binary")

SECTIONS
{
    . = 0x7E00;

    .text : {
        *(.entry)
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
    }
}
