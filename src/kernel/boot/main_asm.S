#define ASM
#include <boot/multiboot.h>
#define MULTIBOOT_CHECKSUM(magic, flags) -((magic) + (flags))

.section multiboot
.align  4
.int    MULTIBOOT_HEADER_MAGIC
.int    MULTIBOOT_HEADER_FLAGS
.int    MULTIBOOT_CHECKSUM(MULTIBOOT_HEADER_MAGIC, MULTIBOOT_HEADER_FLAGS)

.section .text
.global start
start: // kernel entry point
        mov $main_kernel_stack_end, %esp // set main kernel stack (see below)
        // now that we have a stack, we can push multiboot info as parameters
        push %eax // must be MULTIBOOT_BOOTLOADER_MAGIC
        push %ebx // pointer to multiboot info struct
        // thanks to multiboot/GRUB, we should be in the following state:
        // CR0[0]=1 (protected mode enabled), CR0[31]=0 (paging disabled),
        // EFLAGS[9]=0 (interrupts disabled), EFLAGS[17]=0 (VM86 disabled)
        call main // call into our C code and pass multiboot info

.global halt
halt:
        cli // disable interrupts
        hlt // halt CPU
        jmp halt

.global bochs_log
bochs_log:
        enter $0, $0     // see cpuid_asm.S for detailed explanation
        mov 8(%ebp), %al // AL now holds the passed character
        outb %al, $0xE9  // send character to the bochs debug console
        mov $1, %ax      // return a 2-byte 1
        leave
        ret

// Stack for running main to kick off initialization. Later we switch to new
// stacks for running tasks in userspace and handling interrupts in the kernel.
.section .bss
.space  STACK_SIZE - 1
.global main_kernel_stack_end
main_kernel_stack_end:
