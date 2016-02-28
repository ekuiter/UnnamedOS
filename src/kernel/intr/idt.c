/*
 * Interrupt Descriptor Table - defines interrupt vectors
 *
 * http://wiki.osdev.org/Interrupts
 * http://wiki.osdev.org/IDT
 * http://wiki.osdev.org/8259_PIC
 * http://wiki.osdev.org/Interrupt_Service_Routines
 * http://wiki.osdev.org/Selector
 */

#include <common.h>
#include <intr/idt.h>
#include <intr/gdt.h>

#define ISR_INIT(nr) extern void isr_intr_##nr(); \
  idt_init_entry_isr(nr, &isr_intr_##nr)

// the IDTR register pointing to the IDT
typedef struct {
    uint16_t limit; // number of entries in the IDT
    uint32_t base; // where the IDT is located
} __attribute__((packed)) idtr_t;

typedef enum {
    TASK_32 = 0x5, INTR_16, TRAP_16, INTR_32 = 0xE, TRAP_32
} idt_entry_type_t;

// an entry in the IDT (an interrupt vector)
typedef struct {
    uint16_t         func0_15; // pointer to ISR
    uint16_t         selector; // RING0 code segment selector in GDT
    uint8_t          reserved; // always 0
    idt_entry_type_t type : 4; // gate type
    uint8_t          st   : 1; // storage segment flag
    uint8_t          dpl  : 2; // privilege (ring level)
    uint8_t          pr   : 1; // present flag
    uint16_t         func16_31;
} __attribute__((packed)) idt_entry_t;

static idt_entry_t idt[IDT_ENTRIES];

static void idt_init_entry(size_t entry, uintptr_t func, uint32_t selector, idt_entry_type_t type, uint8_t st, uint8_t dpl, uint8_t pr) {
    idt[entry].func0_15  =  func         & 0xFFFF; // 16 lower bits
    idt[entry].func16_31 = (func >> 16)  & 0xFFFF; // 16 higher bits
    idt[entry].selector  = selector;
    idt[entry].reserved  = 0;
    idt[entry].type      = type;
    idt[entry].st        = st;
    idt[entry].dpl       = dpl;
    idt[entry].pr        = pr;
}

static void idt_init_entry_isr(size_t entry, void (*func)()) {
    idt_init_entry(entry, (uintptr_t) func, gdt_get_selector(1), INTR_32, 0, 0, 1);
}

static void idt_load() {
    idtr_t idtr = {.base = (uint32_t) idt, .limit = sizeof(idt) - 1};
    asm volatile("lidt %0" : : "m" (idtr));
}

void idt_init() {
    print("IDT init ... ");
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
    ISR_INIT(0x30);
    idt_load();
    println("%2aok%a.");
}
