.section multiboot
#define MB_MAGIC 0x1badb002
#define MB_FLAGS 0x0
#define MB_CHECKSUM -(MB_MAGIC + MB_FLAGS)

.align  4
.int    MB_MAGIC
.int    MB_FLAGS
.int    MB_CHECKSUM

.section .text

.global _start
_start: // kernel entry point
        mov $kernel_stack, %esp // set the kernel stack (see label below)
        call main // call into our C code

.global halt
halt:
        cli // disable interrupts
        hlt // halt CPU
        jmp halt

// stack
.section .bss
.space  8192
kernel_stack: