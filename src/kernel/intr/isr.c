/*
 * Interrupt Service Routine - handles IRQs, exceptions and syscalls
 *
 * http://www.lowlevel.eu/wiki/Teil_5_-_Interrupts
 * http://www.lowlevel.eu/wiki/ISR
 * http://wiki.osdev.org/Exceptions
 */

#include <common.h>
#include <intr/isr.h>
#include <intr/pic.h>

#define IS_EXCEPTION(intr) (intr <= 0x1F)
#define IS_IRQ(intr)       (intr >= 0x20 && intr <= 0x2F)
#define IS_SYSCALL(intr)   (intr == 0x30)
#define INT_IRQ0  0x20
#define INT_TIMER INT_IRQ0

// registers pushed in idt_isr (see isr_asm.S)
typedef struct {
    uint16_t gs, : 16, fs, : 16, es, : 16, ds, : 16;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax, intr, error, eip;
    uint16_t cs, : 16;
    uint32_t eflags, user_esp, user_ss;
} __attribute__((packed)) cpu_state_t;

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

// ESP points to the CPU state, returns a (possibly new) ESP to allow switching tasks
uint32_t isr_handle_interrupt(uint32_t esp) {
    cpu_state_t* cpu = (cpu_state_t*) esp;
    if (IS_EXCEPTION(cpu->intr)) {
        switch (cpu->intr) {
            default:
                print("%4aEX%02x (EIP=%08x)", cpu->intr, cpu->eip);
                panic("");
        }
    } else if (IS_IRQ(cpu->intr)) {
        switch (cpu->intr) {
            case INT_TIMER: break;
            default:
                print("%2aIRQ%02x%a", cpu->intr - INT_IRQ0);
        }
        pic_send_eoi(cpu->intr);
    } else if (IS_SYSCALL(cpu->intr)) {
        switch (cpu->eax) {
            case 0: break;
            default:
                print("%4aSYS%08x%a", cpu->eax);
        }
    }
    return esp;
}