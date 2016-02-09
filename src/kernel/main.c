#include <common.h>
#include <intr/gdt.h>
#include <intr/cpuid.h>
#include <intr/idt.h>
#include <intr/pic.h>
#include <intr/isr.h>

void main() {
    io_clear();
    println("%15aWelcome!%a");
    gdt_init();
    cpuid_init();
    idt_init();
    pic_init();
    isr_interrupts(1);
    while(1);
}

void _panic(char* msg, char* file, uint32_t line) {
    print("%4a%s at %s:%d", msg, file, line);
    halt();
}
