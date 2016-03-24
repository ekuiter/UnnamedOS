#include <common.h>
#include <boot/multiboot.h>
#include <hardware/cpu/cpuid.h>
#include <hardware/io/keyboard.h>
#include <hardware/io/mouse.h>
#include <hardware/io/ps2.h>
#include <hardware/pit.h>
#include <interrupts/idt.h>
#include <interrupts/isr.h>
#include <interrupts/pic.h>
#include <mem/gdt.h>
#include <mem/pmm.h>
#include <tasks/elf.h>
#include <tasks/task.h>

#define _4KB 0x1000

static void main2(), ps2();

static void handle_keyboard_event(keyboard_event_t e) {
    if (e.ascii && e.pressed)
        print("%9a%c%a", e.ascii);
}

static void handle_mouse_event(mouse_event_t e) {
    size_t old_cursor = io_cursor(IO_COORD(e.screen_x, e.screen_y));
    print(e.left || e.right || e.middle ? "%6a%c%a" : "%9a%c%a", 219);
    io_cursor(old_cursor);
}

void main(multiboot_info_t* mb_info, uint32_t mb_magic) {
    io_clear(io_putchar); // clear any messages GRUB left us
    println("%15aWelcome!%a");
    multiboot_init(mb_info, mb_magic); // Multiboot - info passed by bootloader
    pmm_init(); // Physical Memory Manager - info on free and used memory
    gdt_init(); // Global Descriptor Table - flat memory model
    cpuid_init(); // CPUID - gather information about the CPU
    idt_init(); // Interrupt Descriptor Table - set up ISRs
    pic_init(); // Programmable Interrupt Controller - remap IRQs
    pit_init(50); // Programmable Interval Timer - system clock
    isr_interrupts(1); // enable interrupts
    // hand over further initialization to multitasking-land main2
    task_create_kernel(main2, STACK_SIZE);
    asm volatile("hlt"); // stop execution until the scheduler calls main2
}

static void main2() {
    task_t* ps2_task = task_create_kernel(ps2, _4KB);
    elf_t* elf = multiboot_get_module("/user_template");
    task_t* elf_task = 0;
    if (elf)
        elf_task = task_create_user(elf_load(elf), _4KB, _4KB);
    else
        println("%4aModule not found.%a");
    pmm_dump(0, 1024 * _4KB);
        
    while (keyboard_get_event().keycode != KEY("ESC")) {
        size_t old_cursor = io_cursor(IO_COORD(IO_COLS - 8, IO_ROWS - 1));
        pit_dump_time();
        io_cursor(old_cursor);
        pit_sleep(1000);
    }
    
    task_destroy(ps2_task);
    if (elf_task)
        task_destroy(elf_task);
    if (elf)
        elf_unload(elf);
    println("%15aIt's now safe to turn off your computer. ;)%a");
    halt();
}
    
static void ps2() {
    ps2_init(); // PS/2 Controller - mouse, keyboard and speaker control
    keyboard_register_handler(handle_keyboard_event);
    mouse_register_handler(handle_mouse_event);
    while (1); // TODO: proper task exit (return value/parameters?)
}
