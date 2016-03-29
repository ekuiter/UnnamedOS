ENTRY(main)

SECTIONS {
    . = 0x40000000; /* load at 1 GiB */
    .text : { *(.text) }
    .data : { *(.data) }
    .rodata : { *(.rodata) }
    .bss : { *(.bss) }
}
