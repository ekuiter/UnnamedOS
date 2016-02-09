#include <common.h>
#include <intr/gdt.h>
#include <intr/cpuid.h>
#include <intr/idt.h>
#include <intr/pic.h>
#include <intr/isr.h>

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

void _panic(char* msg, char* file, uint32_t line) {
    io_attr(IO_RED);
    io_putstr(msg);
    io_putstr(" at ");
    io_putstr(file);
    io_putchar(':');
    io_putint(line, 10, 0, 0);
    halt();
}
