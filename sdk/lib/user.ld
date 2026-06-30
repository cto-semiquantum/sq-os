ENTRY(_start)
OUTPUT_FORMAT("elf32-i386")

SECTIONS
{
    /* Load address inside virtual user space (4MB to 8MB) */
    . = 0x00500000;

    .text : {
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
        *(COMMON)
    }
}
