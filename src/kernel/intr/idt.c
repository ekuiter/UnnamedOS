#include <intr/idt.h>
#include <intr/gdt.h>
#include <io/output.h>

/*
 * Interrupt Descriptor Table - defines interrupt vectors
 *
 * http://wiki.osdev.org/Interrupts
 * http://wiki.osdev.org/IDT
 * http://wiki.osdev.org/8259_PIC
 * http://wiki.osdev.org/Interrupt_Service_Routines
 * http://wiki.osdev.org/Selector
 * http://www.lowlevel.eu/wiki/Teil_5_-_Interrupts
 */

#define IDT_ENTRIES 256

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
    uint16_t          func0_15; // pointer to ISR
    uint16_t          selector; // RING0 code segment selector in GDT
    uint8_t           reserved; // always 0
    idt_entry_type_t  type : 4; // gate type
    uint8_t           st   : 1; // storage segment flag
    uint8_t           dpl  : 2; // privilege (ring level)
    uint8_t           pr   : 1; // present flag
    uint16_t          func16_31;

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

static void idt_load() {
    idtr_t idtr = {.base = (uint32_t) idt, .limit = sizeof(idt) - 1};
    asm volatile("lidt %0" : : "m" (idtr));
}

extern void idt_isr();

void idt_init() {
    io_putstr("IDT init ... ");
    for (int i = 0; i < IDT_ENTRIES; i++)
        idt_init_entry(i, (uintptr_t) &idt_isr, gdt_get_selector(1), INTR_32, 0, 0, 1);
    idt_load();
    io_attr(IO_GREEN);
    io_putstr("ok");
    io_attr(IO_DEFAULT);
    io_putstr(".\n");
}

void idt_handle_interrupt() {
    io_putstr("Interrupt!\n");
}
