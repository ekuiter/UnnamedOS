/**
 * @file
 * @addtogroup isr
 * @{
 */

#ifndef INTERRUPTS_ISR_H
#define INTERRUPTS_ISR_H

#include <stdint.h>

#define ISR_EXCEPTION(ex) (0x00 + (ex))  ///< the interrupt vector for an exception
#define ISR_IRQ(irq)      (0x20 + (irq)) ///< the interrupt vector for an IRQ
#define ISR_SYSCALL        0x30          ///< the interrupt vector for the syscall

/** The EFLAGS register. It contains control and status flags. */
typedef union {
    struct {
        uint8_t cf   : 1, ///< carry flag
            reserved : 1, ///< always one
            pf       : 1, ///< parity flag
            : 1, af  : 1, ///< auxiliary carry flag
            : 1, zf  : 1, ///< zero flag
            sf       : 1, ///< sign flag
            tf       : 1, ///< trap flag
            _if      : 1, ///< interrupt flag
            df       : 1, ///< direction flag
            of       : 1, ///< overflow flag
            iopl     : 2, ///< IO privilege level
            nt       : 1, ///< nested task flag
            : 1, rf  : 1, ///< resume flag
            vm       : 1, ///< virtual 8086 mode
            ac       : 1, ///< alignment check
            vif      : 1, ///< virtual interrupt flag
            vip      : 1, ///< virtual interrupt pending
            id       : 1; ///< ID flag
    } __attribute__((packed)) bits; ///< bit field
    uint32_t dword;       ///< useful for casting
} isr_eflags_t;

/// general purpose registers
typedef struct {
    uint32_t edi, ///< destination index register
            esi,  ///< source index register
            ebp,  ///< base pointer register
            esp,  ///< stack pointer register
            ebx,  ///< base register
            edx,  ///< data register
            ecx,  ///< counter register
            eax;  ///< accumulator register
} __attribute__((packed)) isr_registers_t;
    
/**
 * The CPU's state when an interrupt occurs. For every task, it is saved on a
 * separate stack and restored when that particular task is switched to.
 * @see isr_common in isr_asm.S
 */
typedef struct {
    // segment registers
    uint16_t gs,      ///< GS segment register
            : 16, fs, ///< FS segment register
            : 16, es, ///< ES segment register
            : 16, ds, ///< DS segment register
            : 16;
    /** The general purpose registers. We do not use the ESP pushed here by
     * pusha at all because popa ignores it. */
    isr_registers_t r;
    uint32_t intr,    ///< the interrupt vector of the fired interrupt
            error,    ///< hold an error code if an error occured, otherwise 0
            eip;      ///< the instruction to return to after the interrupt
    uint16_t cs,      ///< the code segment to return to after the interrupt
            : 16;
    isr_eflags_t eflags; ///< the EFLAGS register before the interrupt was fired
    uint32_t user_esp, ///< stack pointer (only pushed and popped in user space)
            user_ss;   ///< stack segment (only pushed and popped in user space)
    // These are only popped upon entering a VM86 task.
    uint16_t vm86_es,      ///< ES register (only pushed and popped in VM86 mode)
            : 16, vm86_ds, ///< DS register (only pushed and popped in VM86 mode)
            : 16, vm86_fs, ///< FS register (only pushed and popped in VM86 mode)
            : 16, vm86_gs, ///< GS register (only pushed and popped in VM86 mode)
            : 16;
} __attribute__((packed)) cpu_state_t;

/**
 * Handles a specific interrupt. Is registered with isr_register_handler().
 * @param cpu the CPU state
 * @return a (possibly altered) CPU state
 * @see isr_handle_interrupt
 */
typedef cpu_state_t* (*isr_handler_t)(cpu_state_t* cpu);

/**
 * Handles a specific syscall. Is registered with isr_register_syscall().
 * Unused parameters may be omitted because isr_register_syscall() works with
 * void*. The parameters' and return value's types may differ from uint32_t,
 * as long as they are not larger than 4 bytes (e.g. structs).
 * @param ebx the syscall's 1st parameter
 * @param ecx the syscall's 2nd parameter
 * @param edx the syscall's 3rd parameter
 * @param esi the syscall's 4th parameter
 * @param edi the syscall's 5th parameter
 * @param cpu A reference to the CPU state / stack pointer. (This is a pointer
 *            to a pointer, so we can switch tasks from within a syscall!)
 * @return The syscall's return value. void is not allowed as type.
 */
typedef uint32_t (*isr_syscall_t)(uint32_t ebx, uint32_t ecx, uint32_t edx,
        uint32_t esi, uint32_t edi, cpu_state_t** cpu);

uint8_t isr_enable_interrupts(uint8_t enable);
uint8_t isr_get_interrupts();
void isr_register_handler(size_t intr, isr_handler_t handler);
void isr_register_syscall(size_t eax, void* syscall);
void isr_dump_cpu(cpu_state_t* cpu);
void isr_init();

#endif

/// @}
