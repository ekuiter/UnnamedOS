/*
 * Interrupt Service Routine - handles IRQs, exceptions and syscalls
 *
 * http://www.lowlevel.eu/wiki/Teil_5_-_Interrupts
 * http://www.lowlevel.eu/wiki/ISR
 * http://wiki.osdev.org/Exceptions
 */

#include <common.h>
#include <interrupts/isr.h>
#include <interrupts/idt.h>
#include <interrupts/pic.h>
#include <hardware/pit.h>
#include <hardware/io/ps2.h>

#define IS_EXCEPTION(intr) ((intr) <= 0x1F)
#define IS_IRQ(intr)       ((intr) >= 0x20 && intr <= 0x2F)
#define IS_SYSCALL(intr)   ((intr) == 0x30)

static isr_handler_t handlers[IDT_ENTRIES] = {0};

void isr_interrupts(uint8_t enable) {
    if (enable) {
        print("Enabling interrupts ... ");
        asm volatile("sti");
    } else {
        print("Disabling interrupts ... ");
        asm volatile("cli");
    }
    println("%2aok%a.");
}

void isr_register_handler(uint8_t intr, isr_handler_t handler) {
    if (intr >= IDT_ENTRIES) {
        println("%4ainterrupt vector %d not allowed%a", intr);
        return;
    }
    handlers[intr] = handler;
}

// cpu has two functions here - as a pointer (CPU state) and as a value (ESP):
// cpu points to the CPU state and is the ESP pushed in isr_asm.S (the former
// stack pointer which points to the current task's CPU state, beginning with GS).
// The pointer returned by this function is a (possibly new) ESP if we want to
// switch tasks (then we need to make sure that ESP points to a valid CPU state).
// If we don't want to switch tasks, we just return the ESP unchanged.
cpu_state_t* isr_handle_interrupt(cpu_state_t* cpu) {
    if (handlers[cpu->intr])
        cpu = handlers[cpu->intr](cpu); // execute a handler if registered
    else {
        if (IS_EXCEPTION(cpu->intr))
            panic("%4aEX%02x (EIP=%08x)", cpu->intr, cpu->eip);
        if (IS_IRQ(cpu->intr))
            print("%2aIRQ%d%a", cpu->intr - ISR_IRQ(0));
        if (IS_SYSCALL(cpu->intr))
            print("%4aSYS%08x%a", cpu->eax);
    }
    if (IS_IRQ(cpu->intr))
        pic_send_eoi(cpu->intr);
    return cpu;
}