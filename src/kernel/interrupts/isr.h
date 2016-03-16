#ifndef INTERRUPTS_ISR_H
#define INTERRUPTS_ISR_H

#include <stdint.h>

#define ISR_EXCEPTION(ex) (0x00 + ex)
#define ISR_IRQ(irq)      (0x20 + irq)
#define ISR_SYSCALL        0x30

// registers pushed in idt_isr (see isr_asm.S)
typedef struct {
    uint16_t gs, : 16, fs, : 16, es, : 16, ds, : 16;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax, intr, error, eip;
    uint16_t cs, : 16;
    uint32_t eflags, user_esp, user_ss;
} __attribute__((packed)) cpu_state_t;

// every handler is passed the CPU state/ESP and returns a (possibly altered) ESP
typedef cpu_state_t* (*isr_handler_t)(cpu_state_t* cpu);

void isr_interrupts(uint8_t enable);
void isr_register_handler(uint8_t intr, isr_handler_t handler);

#endif
