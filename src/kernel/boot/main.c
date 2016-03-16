#include <common.h>
#include <boot/multiboot.h>
#include <mem/gdt.h>
#include <hardware/cpu/cpuid.h>
#include <interrupts/idt.h>
#include <interrupts/pic.h>
#include <hardware/pit.h>
#include <interrupts/isr.h>
#include <hardware/io/ps2.h>
#include <hardware/io/keyboard.h>
#include <hardware/io/mouse.h>

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

void main(multiboot_info_t* mb_info, uint32_t mb_magic) {
    io_clear(io_putchar); // clear any messages GRUB left us
    println("%15aWelcome!%a");
    multiboot_init(mb_info, mb_magic); // Multiboot - info passed by bootloader
    gdt_init(); // Global Descriptor Table - flat memory model
    cpuid_init(); // CPUID - gather information about the CPU
    idt_init(); // Interrupt Descriptor Table - set up ISRs
    pic_init(); // Programmable Interrupt Controller - remap IRQs
    pit_init(20); // Programmable Interval Timer - system clock
    isr_interrupts(1); // enable interrupts
    ps2_init(); // PS/2 Controller - mouse, keyboard and speaker control
    keyboard_register_handler(handle_keyboard_event);
    mouse_register_handler(handle_mouse_event);
    while(1);
    while(1) {
        io_cursor(IO_COORD(IO_COLS - 8, IO_ROWS - 1));
        pit_time();
        pit_sleep(1000);
    }
}
