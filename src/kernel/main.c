#include <common.h>
#include <intr/gdt.h>
#include <intr/cpuid.h>
#include <intr/idt.h>
#include <intr/pic.h>
#include <intr/pit.h>
#include <intr/isr.h>

void main() {
    io_clear();
    println("%15aWelcome!%a");
    gdt_init();
    cpuid_init();
    idt_init();
    pic_init();
    pit_init(20);
    isr_interrupts(1);
    //print("%d", 1/0);
    while(1) {
        io_cursor(IO_COORD(IO_COLS - 6, IO_ROWS - 1));
        print("%6d", pit_seconds());
        pit_sleep(1000);
    }
}
