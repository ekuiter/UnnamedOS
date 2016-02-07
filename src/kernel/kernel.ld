/* os entry point is _start in start.S */
ENTRY(_start)

/* defines the sections of the kernel file */
SECTIONS
{
    /* load at 1 MiB */
    . = 0x100000;

    /* these constants save section start and end addresses */
    kernelStart = .;
    
    /* code section */
    .text : {
        multibootStart = .;
        *(multiboot) /* inserts the multiboot header - see start.S .section multiboot */
        multibootEnd = .;
        textStart = .;
        *(.text) /* this contains the code */
        textEnd = .;
    }
    
    /* global variables */
    .data ALIGN(0x1000) : { /* align to 4 KiB bounds */
        dataStart = .;
        *(.data)
        dataEnd = .;
    }
    
    /* strings */
    .rodata ALIGN(0x1000) : {
        rodataStart = .;
        *(.rodata)
        rodataEnd = .;
    }
    
    /* bss section - contains uninitialized variables / arrays */
    .bss ALIGN(0x1000) : {
        bssStart = .;
        *(.bss)
        bssEnd = .;
    }

    /* make the kernel file aligned to 4 KiB */
    . = ALIGN(0x1000);
    kernelEnd = .;
}
