#include <stdint.h>
#include <intr/gdt.h>
#include <io/output.h>

void main() {
    gdt_init();
    io_clear();
    io_putstr("Hello World!");
    io_attr(0x4);
    io_putstr(" ;)\n");
    io_attr(0x2);
    io_putint(0x12345678, 16, 0, 0);
}
