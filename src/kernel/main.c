#include <stdint.h>
#include <intr/gdt.h>
#include <intr/idt.h>
#include <intr/cpuid.h>
#include <io/output.h>

void main() {
    io_clear();
    io_putstr("Welcome!\n");
    gdt_init();
    cpuid_init();
    idt_init();
    asm volatile("sti");
    io_putstr("Hello World!\n");
    while(1);
}
