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
#include <tasks/task.h>

static void main2(), a(), b();
static task_t main2_task, a_task, b_task;
static task_stack_t main2_stack[STACK_SIZE], a_user_stack[0x400],
        a_kernel_stack[0x400], b_user_stack[0x400], b_kernel_stack[0x400];

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
    gdt_init(); // Global Descriptor Table - flat memory model
    cpuid_init(); // CPUID - gather information about the CPU
    idt_init(); // Interrupt Descriptor Table - set up ISRs
    pic_init(); // Programmable Interrupt Controller - remap IRQs
    pit_init(20); // Programmable Interval Timer - system clock
    isr_interrupts(1); // enable interrupts
    ps2_init(); // PS/2 Controller - mouse, keyboard and speaker control
    keyboard_register_handler(handle_keyboard_event);
    mouse_register_handler(handle_mouse_event);
    // hand over further initialization to multitasking-land main2
    task_create_kernel(&main2_task, (uintptr_t) main2, main2_stack, sizeof(main2_stack));
    asm volatile("hlt"); // stop execution until the scheduler calls main2
}

static void main2() {
    task_create_user(&a_task, (uintptr_t) a, a_kernel_stack, sizeof(a_kernel_stack),
            a_user_stack, sizeof(a_user_stack));
    task_create_user(&b_task, (uintptr_t) b, b_kernel_stack, sizeof(b_kernel_stack),
            b_user_stack, sizeof(b_user_stack));
    while(1) {
        size_t old_cursor = io_cursor(IO_COORD(IO_COLS - 8, IO_ROWS - 1));
        pit_dump_time();
        io_cursor(old_cursor);
        pit_sleep(1000);
    }
}

static void a() {
    while (1) {
        print("A");
        pit_sleep(50);
    }
}

static void b() {
    while (1) {
        print("B");
        pit_sleep(50);
    }
}
