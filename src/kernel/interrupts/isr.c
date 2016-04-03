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
#include <interrupts/syscall.h>
#include <hardware/pit.h>
#include <hardware/io/ps2.h>
#include <syscall.h>

#define IS_EXCEPTION(intr) ((intr) <= 0x1F)
#define IS_IRQ(intr)       ((intr) >= 0x20 && intr <= 0x2F)
#define IS_SYSCALL(intr)   ((intr) == 0x30)

static isr_handler_t handlers[IDT_ENTRIES] = {0};
static void* syscalls[SYSCALL_NUMBER] = {0};

void isr_enable_interrupts(uint8_t enable) {
    if (enable) {
        asm volatile("sti");
    } else
        asm volatile("cli");
}

void isr_register_handler(size_t intr, isr_handler_t handler) {
    if (intr >= IDT_ENTRIES) {
        println("%4ainterrupt vector %d not allowed%a", intr);
        return;
    }
    handlers[intr] = handler;
}

void isr_register_syscall(size_t id, void* syscall) {
    if (id >= SYSCALL_NUMBER) {
        println("%4asyscall %d not allowed%a", id);
        return;
    }
    syscalls[id] = syscall;
}

// cpu has two functions here - as a pointer (CPU state) and as a value (ESP):
// cpu points to the CPU state and is the ESP pushed in isr_asm.S (the former
// stack pointer which points to the current task's CPU state, beginning with GS).
// The pointer returned by this function is a (possibly new) ESP if we want to
// switch tasks (then we need to make sure that ESP points to a valid CPU state).
// If we don't want to switch tasks, we just return the ESP unchanged.
cpu_state_t* isr_handle_interrupt(cpu_state_t* cpu) {
    uint32_t intr = cpu->intr; // save intr so we might change the cpu state
    if (handlers[intr])
        cpu = handlers[intr](cpu); // execute a handler if registered
    else {
        if (IS_EXCEPTION(intr))
            panic("%4aEX%02x (EIP=%08x)", intr, cpu->eip);
        if (IS_IRQ(intr))
            print("%2aIRQ%d%a", intr - ISR_IRQ(0));
        if (IS_SYSCALL(intr))
            print("%4aSYS%08x%a", cpu->eax);
    }
    if (IS_IRQ(intr))
        pic_send_eoi(intr);
    return cpu;
}

void isr_dump_cpu(cpu_state_t* cpu) {
    logln("ISR", "uss=%08x usp=%08x efl=%08x  cs=    %04x eip=%08x err=%08x "
            "int=%08x eax=%08x ecx=%08x edx=%08x",
            cpu->user_ss, cpu->user_esp, cpu->eflags, cpu->cs, cpu->eip,
            cpu->error, cpu->intr, cpu->eax, cpu->ecx, cpu->edx);
    logln("ISR", "ebx=%08x esp=%08x ebp=%08x esi=%08x edi=%08x  "
            "ds=    %04x  es=    %04x  fs=    %04x  gs=    %04x",
            cpu->ebx, cpu->esp, cpu->ebp, cpu->esi, cpu->edi,
            cpu->ds, cpu->es, cpu->fs, cpu->gs);
}

static cpu_state_t* isr_handle_syscall(cpu_state_t* cpu) {
    if (cpu->eax < SYSCALL_NUMBER && syscalls[cpu->eax])
        cpu->eax = ((isr_syscall_t) syscalls[cpu->eax])
            (cpu->ebx, cpu->ecx, cpu->edx, cpu->esi, cpu->edi, &cpu);
    else
        println("%4aUnknown syscall %08x%a", cpu->eax);
    return cpu;
}

void isr_init() {
    print("ISR init ... ");
    isr_register_handler(0x30, isr_handle_syscall);
    syscall_init();
    isr_enable_interrupts(1);
    println("%2aok%a.");
}
