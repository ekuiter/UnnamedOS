/**
 * @file
 * @addtogroup isr
 * @{
 * Interrupt Service Routine
 * 
 * An ISR is responsible for handling IRQs, exceptions and syscalls. Here only
 * one common ISR is used, isr_handle_interrupt. It dispatches interrupts to
 * handlers (isr_handler_t) registered by isr_register_handler.
 * @see isr_asm.S
 * @see http://www.lowlevel.eu/wiki/Teil_5_-_Interrupts
 * @see http://www.lowlevel.eu/wiki/ISR
 * @see http://wiki.osdev.org/Exceptions
 */

#include <common.h>
#include <interrupts/isr.h>
#include <interrupts/idt.h>
#include <interrupts/pic.h>
#include <interrupts/syscall.h>
#include <hardware/pit.h>
#include <hardware/io/ps2.h>
#include <syscall.h>

/// whether the given interrupt vector is an exception
#define IS_EXCEPTION(intr) ((intr) <= 0x1F)
/// whether the given interrupt vector is an IRQ
#define IS_IRQ(intr)       ((intr) >= 0x20 && intr <= 0x2F)
/// whether the given interrupt vector is the syscall
#define IS_SYSCALL(intr)   ((intr) == 0x30)

/// a table that associates interrupt vectors with handlers
static isr_handler_t handlers[IDT_ENTRIES] = {0};
/// a table that associates syscall IDs with handlers
static void* syscalls[SYSCALL_NUMBER] = {0};

/**
 * Enables or disables interrupts. Only acts if necessary.
 * @param enable the new interrupt flag
 * @return interrupt flag before calling isr_enable_interrupts()
 */
uint8_t isr_enable_interrupts(uint8_t enable) {
    uint8_t old_interrupts = isr_get_interrupts();
    if (enable && !old_interrupts) {
        asm volatile("sti");
    } else if (!enable && old_interrupts)
        asm volatile("cli");
    return old_interrupts;
}

/**
 * Returns whether interrupts are enabled or disabled.
 * @return interrupt flag
 */
uint8_t isr_get_interrupts() {
    isr_eflags_t eflags;
    asm volatile("pushfl; pop %%eax; mov %%eax, %0" : "=r" (eflags.dword));
    return eflags.bits._if;
}

/**
 * Registers a handler to call whenever a given interrupt is fired.
 * @param intr    the interrupt vector that will be handled
 * @param handler the function to call
 */
void isr_register_handler(size_t intr, isr_handler_t handler) {
    if (intr >= IDT_ENTRIES) {
        println("%4ainterrupt vector %d not allowed%a", intr);
        return;
    }
    handlers[intr] = handler;
}

/**
 * Registers a syscall handler to call whenever a specified syscall is requested.
 * @param id      the ID of the syscall that will be handled
 * @param syscall the function to call (this should be an isr_syscall_t but
 *                parameters can be omitted if desired, so we pass a void*)
 */
void isr_register_syscall(size_t id, void* syscall) {
    if (id >= SYSCALL_NUMBER) {
        println("%4asyscall %d not allowed%a", id);
        return;
    }
    syscalls[id] = syscall;
}

/**
 * Handles all interrupts. This function is called whenever an interrupt is
 * fired. It distinguishes which action to perform (call a handler, panic etc.)
 * and, for IRQs, notifies the PIC that this interrupt has been handled.
 * @param cpu
 * cpu has two functions here - as a pointer (CPU state) and as a value (ESP):<br>
 * cpu points to the CPU state and is the ESP pushed in isr_asm.S (the former
 * stack pointer which points to the current task's CPU state, beginning with GS).
 * @return
 * The pointer returned by this function is a (possibly new) ESP if we want to
 * switch tasks (then we need to make sure that ESP points to a valid CPU state).<br>
 * If we don't want to switch tasks, we just return the ESP unchanged.
 * @see isr_common in isr_asm.S
 */
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
            print("%4aSYS%08x%a", cpu->r.eax);
    }
    if (IS_IRQ(intr))
        pic_send_eoi(intr);
    return cpu;
}

/**
 * Dumps a CPU state. The dump is logged.
 * @param cpu the CPU state to dump
 */
void isr_dump_cpu(cpu_state_t* cpu) {
    logln("ISR", "uss=%08x usp=%08x efl=%08x  cs=    %04x eip=%08x err=%08x "
            "int=%08x eax=%08x ecx=%08x edx=%08x",
            cpu->user_ss, cpu->user_esp, cpu->eflags, cpu->cs, cpu->eip,
            cpu->error, cpu->intr, cpu->r.eax, cpu->r.ecx, cpu->r.edx);
    logln("ISR", "ebx=%08x esp=%08x ebp=%08x esi=%08x edi=%08x  "
            "ds=    %04x  es=    %04x  fs=    %04x  gs=    %04x",
            cpu->r.ebx, cpu->r.esp, cpu->r.ebp, cpu->r.esi, cpu->r.edi,
            cpu->ds, cpu->es, cpu->fs, cpu->gs);
}

/**
 * Handles all syscalls. Calls a syscall handler if registered. 
 * @param cpu the current CPU state, see isr_handle_interrupt()
 * @return the new CPU state (if a task switch is desired)
 */
static cpu_state_t* isr_handle_syscall(cpu_state_t* cpu) {
    if (cpu->r.eax < SYSCALL_NUMBER && syscalls[cpu->r.eax]) {
        cpu_state_t* old_cpu = cpu;
        uint32_t ret = ((isr_syscall_t) syscalls[cpu->r.eax])
            (cpu->r.ebx, cpu->r.ecx, cpu->r.edx, cpu->r.esi, cpu->r.edi, &cpu);
        if (cpu == old_cpu)   // change EAX only if we didn't switch tasks,
            cpu->r.eax = ret; // otherwise tasks might run havoc!
    } else
        println("%4aUnknown syscall %08x%a", cpu->r.eax);
    return cpu;
}

/// Initializes syscalls and enables interrupts.
void isr_init() {
    print("ISR init ... ");
    isr_register_handler(ISR_SYSCALL, isr_handle_syscall);
    syscall_init();
    isr_enable_interrupts(1);
    println("%2aok%a.");
}

/// @}
