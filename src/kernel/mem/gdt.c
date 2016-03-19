/*
 * Global Descriptor Table - defines memory segments (here: a flat memory model)
 *
 * http://www.lowlevel.eu/wiki/Global_Descriptor_Table
 * http://wiki.osdev.org/Segmentation
 * http://wiki.osdev.org/GDT
 * http://wiki.osdev.org/GDT_Tutorial
 * https://en.wikibooks.org/wiki/X86_Assembly/Global_Descriptor_Table
 * https://stackoverflow.com/questions/23978486/far-jump-in-gdt-in-bootloader/23979175#23979175
 */

#include <common.h>
#include <mem/gdt.h>
#include <tasks/tss.h>

// the GDTR register pointing to the GDT
typedef struct {
    uint16_t limit; // number of entries in the GDT
    uint32_t base; // where the GDT is located
} __attribute__((packed)) gdtr_t;

// a selector in the GDT
typedef union {
    struct {
        uint16_t dpl : 2, idt : 1, entry : 13;
    } __attribute__((packed)) bits;
    uint16_t word;
} gdt_selector_t;

static gdt_entry_t gdt[GDT_ENTRIES];

void gdt_init_entry(size_t entry, uint32_t base, uint32_t limit) {
    gdt[entry].base0_23   =  base         & 0xFFFFFF; // 24 lower bits
    gdt[entry].base24_31  = (base >> 24)  & 0xFF;     // 8 higher bits
    gdt[entry].limit0_15  =  limit        & 0xFFFF;   // 16 lower bits
    gdt[entry].limit16_19 = (limit >> 16) & 0xF;      // 4 higher bits
    gdt[entry].reserved   = 0;
}

static void gdt_load() {
    gdtr_t gdtr = {.base = (uint32_t) gdt, .limit = sizeof(gdt) - 1};
    asm volatile("lgdt %0" : : "m" (gdtr));
}

extern void gdt_flush();

void gdt_init() {
    print("GDT init ... ");
    gdt_init_entry(0, 0, 0); // null descriptor
    gdt_init_entry(GDT_RING0_CODE_SEG, 0, 0xFFFFF); // kernel code segment (see gdt_flush_code_segment in gdt_asm.S)
    gdt_init_entry(GDT_RING0_DATA_SEG, 0, 0xFFFFF); // kernel data segment (see gdt_flush_data_segment in gdt_asm.S)
    gdt_init_entry(GDT_RING3_CODE_SEG, 0, 0xFFFFF); // user code segment
    gdt_init_entry(GDT_RING3_DATA_SEG, 0, 0xFFFFF); // user data segment
    gdt[0].ac = 0; gdt[0].rw = 0; gdt[0].dc = 0; gdt[0].ex = 0; gdt[0].dt = 0; gdt[0].dpl = 0; gdt[0].pr = 0; gdt[0].sz = 0; gdt[0].gr = 0;
    gdt[1].ac = 0; gdt[1].rw = 1; gdt[1].dc = 0; gdt[1].ex = 1; gdt[1].dt = 1; gdt[1].dpl = 0; gdt[1].pr = 1; gdt[1].sz = 1; gdt[1].gr = 1;
    gdt[2].ac = 0; gdt[2].rw = 1; gdt[2].dc = 0; gdt[2].ex = 0; gdt[2].dt = 1; gdt[2].dpl = 0; gdt[2].pr = 1; gdt[2].sz = 1; gdt[2].gr = 1;
    gdt[3].ac = 0; gdt[3].rw = 1; gdt[3].dc = 0; gdt[3].ex = 1; gdt[3].dt = 1; gdt[3].dpl = 3; gdt[3].pr = 1; gdt[3].sz = 1; gdt[3].gr = 1;
    gdt[4].ac = 0; gdt[4].rw = 1; gdt[4].dc = 0; gdt[4].ex = 0; gdt[4].dt = 1; gdt[4].dpl = 3; gdt[4].pr = 1; gdt[4].sz = 1; gdt[4].gr = 1;
    tss_init(gdt);
    gdt_load();
    gdt_flush();
    tss_load();
    println("%2aok%a.");
}

uint16_t gdt_get_selector(size_t entry) {
    gdt_selector_t selector = {.bits = {.dpl = gdt[entry].dpl, .entry = entry}};
    return (uint16_t) selector.word; // selector used in IDT and segment registers
}
