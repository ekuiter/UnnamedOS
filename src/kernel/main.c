#include <stdint.h>
#include <intr/gdt.h>
#include <intr/cpuid.h>
#include <intr/idt.h>
#include <intr/pic.h>
#include <intr/isr.h>
#include <io/output.h>

void main() {
    io_clear();
    io_putstr("Welcome!\n");
    gdt_init();
    cpuid_init();
    idt_init();
    pic_init();
    isr_interrupts(1);
    io_putstr("Hello World!\n");
    while(1);
}
