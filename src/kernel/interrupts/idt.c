/**
 * @file
 * @addtogroup idt
 * @{
 * Interrupt Descriptor Table
 * 
 * The IDT maps interrupt vectors to interrupt service routines (ISRs) to
 * allow handling of exceptions, IRQs (Interrupt Requests) and system calls.
 * Here all interrupt vectors are mapped to the same common interrupt handler.
 * @see http://wiki.osdev.org/Interrupts
 * @see http://wiki.osdev.org/IDT
 * @see http://wiki.osdev.org/8259_PIC
 * @see http://wiki.osdev.org/Interrupt_Service_Routines
 * @see http://wiki.osdev.org/Selector
 */

#include <common.h>
#include <interrupts/idt.h>
#include <mem/gdt.h>

/**
 * shorthand for initializing an ISR
 * @see isr_intr in isr_asm.S
 */
#define ISR_INIT(nr) extern void isr_intr_##nr(); \
  idt_init_entry_isr((nr), &isr_intr_##nr, 0)

/// the IDTR register pointing to the IDT
typedef struct {
    uint16_t limit; ///< number of entries in the IDT
    uint32_t base;  ///< where the IDT is located
} __attribute__((packed)) idtr_t;

/** Different types of interrupt vectors. Here we only use 32-bit interrupt gates. */
typedef enum {
    TASK_32 = 0x5, INTR_16, TRAP_16, INTR_32 = 0xE, TRAP_32
} idt_entry_type_t;

/** An entry in the IDT. This maps an interrupt vector to an ISR. */
typedef struct {
    uint16_t         func0_15;  ///< pointer to ISR (byte 1-2)
    uint16_t         selector;  ///< RING0 code segment selector in GDT
    uint8_t          reserved;  ///< always 0
    idt_entry_type_t type : 4;  ///< gate type
    uint8_t          st   : 1;  ///< storage segment flag
    uint8_t          dpl  : 2;  ///< privilege (ring level)
    uint8_t          pr   : 1;  ///< present flag
    uint16_t         func16_31; ///< pointer to ISR (byte 3-4)
} __attribute__((packed)) idt_entry_t;

/**
 * The IDT itself. Like the GDT, it is located inside the kernel. It is
 * important that the IDT is always mapped into the same place in memory
 * when using paging, otherwise interrupts lead to triple faults.
 * @see vmm_init
 */
static idt_entry_t idt[IDT_ENTRIES];

/**
 * Initializes an IDT entry.
 * @param entry    index into the IDT
 * @param func     pointer to ISR
 * @param selector code segment of ISR, usually the kernel code segment
 * @param type     interrupt vector type, usually a 32-bit interrupt gate
 * @param st       storage segment flag
 * @param dpl      maximum allowed privilege level (0=kernel, 3=user space)
 * @param pr       present flag
 */
static void idt_init_entry(size_t entry, uintptr_t func, uint32_t selector,
        idt_entry_type_t type, uint8_t st, uint8_t dpl, uint8_t pr) {
    idt[entry].func0_15  =  func         & 0xFFFF; // 16 lower bits
    idt[entry].func16_31 = (func >> 16)  & 0xFFFF; // 16 higher bits
    idt[entry].selector  = selector;
    idt[entry].reserved  = 0;
    idt[entry].type      = type;
    idt[entry].st        = st;
    idt[entry].dpl       = dpl;
    idt[entry].pr        = pr;
}

/**
 * Shorthand for initializing an ISR.
 * @param entry index into the IDT
 * @param func  pointer to ISR
 * @param dpl   maximum allowed privilege level (0=kernel, 3=user space)
 */
static void idt_init_entry_isr(size_t entry, void (*func)(), uint8_t dpl) {
    idt_init_entry(entry, (uintptr_t) func,
            gdt_get_selector(GDT_RING0_CODE_SEG), INTR_32, 0, dpl, 1);
}

/// Loads the IDT into the IDTR register.
static void idt_load() {
    idtr_t idtr = {.base = (uint32_t) idt, .limit = sizeof(idt) - 1};
    asm volatile("lidt %0" : : "m" (idtr));
}

/// Initializes the IDT.
void idt_init() {
    print("IDT init ... ");
    /// Sets up all the exceptions and IRQs as kernel-only (RING0) interrupts.
    ISR_INIT(0x00); ISR_INIT(0x01); ISR_INIT(0x02); ISR_INIT(0x03);
    ISR_INIT(0x04); ISR_INIT(0x05); ISR_INIT(0x06); ISR_INIT(0x07);
    ISR_INIT(0x08); ISR_INIT(0x09); ISR_INIT(0x0A); ISR_INIT(0x0B);
    ISR_INIT(0x0C); ISR_INIT(0x0D); ISR_INIT(0x0E); ISR_INIT(0x0F);
    ISR_INIT(0x10); ISR_INIT(0x11); ISR_INIT(0x12); ISR_INIT(0x13);
    ISR_INIT(0x14); ISR_INIT(0x15); ISR_INIT(0x16); ISR_INIT(0x17);
    ISR_INIT(0x18); ISR_INIT(0x19); ISR_INIT(0x1A); ISR_INIT(0x1B);
    ISR_INIT(0x1C); ISR_INIT(0x1D); ISR_INIT(0x1E); ISR_INIT(0x1F);
    ISR_INIT(0x20); ISR_INIT(0x21); ISR_INIT(0x22); ISR_INIT(0x23);
    ISR_INIT(0x24); ISR_INIT(0x25); ISR_INIT(0x26); ISR_INIT(0x27);
    ISR_INIT(0x28); ISR_INIT(0x29); ISR_INIT(0x2A); ISR_INIT(0x2B);
    ISR_INIT(0x2C); ISR_INIT(0x2D); ISR_INIT(0x2E); ISR_INIT(0x2F);
    /// Sets up 0x30 as a syscall interrupt which might be called from RING3.
    extern void isr_intr_0x30();
    idt_init_entry_isr(0x30, &isr_intr_0x30, 3);
    idt_load();
    println("%2aok%a.");
}

/// @}
