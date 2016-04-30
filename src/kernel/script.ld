/*
 * Linker script for kernel
 *
 * https://sourceware.org/binutils/docs/ld/Simple-Example.html
 * http://wiki.osdev.org/Linker_Scripts
 */

ENTRY(start) /* os entry point is start in main_asm.S */

SECTIONS { /* defines the sections of the kernel file */
    . = 0x400000; /* load at 4 MiB, leaving space for VM86 tasks */

    kernel_start = .; /* these constants save section start and end addresses */
    .text : { /* code section */
        *(multiboot) /* inserts the multiboot header - see main_asm.S */
        *(.text) /* this contains the code */
    }
    
    .data ALIGN(0x1000) : { /* writable globals, align to 4 KiB bounds */
        *(.data)
    }
    
    .rodata ALIGN(0x1000) : { /* read-only globals (e.g. strings) */
        *(.rodata)
    }
    
    .bss ALIGN(0x1000) : { /* BSS section - contains uninitialized globals */
        *(.bss)
    }

    . = ALIGN(0x1000); /* make the kernel file aligned to 4 KiB */
    kernel_end = .;
}
