#ifndef INTERRUPTS_ISR_H
#define INTERRUPTS_ISR_H

#include <stdint.h>

#define ISR_EXCEPTION(ex) (0x00 + ex)
#define ISR_IRQ(irq)      (0x20 + irq)
#define ISR_SYSCALL        0x30

// registers pushed in idt_isr (see isr_asm.S)
typedef struct {
    // segment registers
    uint16_t gs, : 16, fs, : 16, es, : 16, ds, : 16;
    // We do not use the ESP pushed here by pusha at all because popa ignores it.
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax, intr, error, eip;
    uint16_t cs, : 16;
    union { // EFLAGS register - contains control and status flags
        struct {
            uint8_t cf : 1, reserved : 1, pf : 1, : 1, af : 1, : 1, zf : 1, sf : 1,
                tf : 1, _if : 1, df : 1, of : 1, iopl : 2, nt : 1, : 1, rf : 1,
                vm : 1, ac : 1, vif : 1, vip : 1, id : 1;
        } __attribute__((packed)) bits;
        uint32_t dword;
    } eflags;
    // These are only pushed when we came from userspace and only then also popped.
    uint32_t user_esp, user_ss;
} __attribute__((packed)) cpu_state_t;

// every handler is passed the CPU state/ESP and returns a (possibly altered) ESP
typedef cpu_state_t* (*isr_handler_t)(cpu_state_t* cpu);

void isr_enable_interrupts(uint8_t enable);
void isr_register_handler(uint8_t intr, isr_handler_t handler);
void isr_dump_cpu(cpu_state_t* cpu);

#endif
