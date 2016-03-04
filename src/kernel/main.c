#include <common.h>
#include <intr/gdt.h>
#include <intr/cpuid.h>
#include <intr/idt.h>
#include <intr/pic.h>
#include <intr/pit.h>
#include <intr/isr.h>
#include <io/ps2.h>
#include <io/keyboard.h>
#include <io/mouse.h>

static void handle_keyboard_event(keyboard_event_t e) {
    if (e.ascii && e.pressed)
        print("%9a%c%a", e.ascii);
}

static void handle_mouse_event(mouse_event_t e) {
    io_cursor(IO_COORD(e.screen_x, e.screen_y));
    if (e.left || e.right || e.middle)
        print("%6a%c%a", 219);
    else
        print("%9a%c%a", 219);
}

void main() {
    io_clear(); // clear any messages GRUB left us
    println("%15aWelcome!%a");
    gdt_init(); // Global Descriptor Table - flat memory model
    cpuid_init(); // CPUID - gather information about the CPU
    idt_init(); // Interrupt Descriptor Table - set up ISRs
    pic_init(); // Programmable Interrupt Controller - remap IRQs
    pit_init(20); // Programmable Interval Timer - system clock
    isr_interrupts(1); // enable interrupts
    ps2_init(); // PS/2 Controller - mouse, keyboard and speaker control
    //speaker_test();
    //print("%d", 1/0);
    keyboard_register_handler(handle_keyboard_event);
    mouse_register_handler(handle_mouse_event);
    while(1);
    while(1) {
        io_cursor(IO_COORD(IO_COLS - 8, IO_ROWS - 1));
        pit_time();
        pit_sleep(1000);
    }
}
