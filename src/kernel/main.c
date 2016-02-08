#include <stdint.h>
#include <intr/gdt.h>
#include <intr/cpuid.h>
#include <io/output.h>

void main() {
    io_clear();
    io_putstr("Welcome!\n");
    gdt_init();
    cpuid_init();
    io_putstr("Hello World!");
    io_attr(0x4);
    io_putstr(" ;)\n");
    io_attr(0x2);
    io_putint(0x12345678, 16, 0, 0);
}
