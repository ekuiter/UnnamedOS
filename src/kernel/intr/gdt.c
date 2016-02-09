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
#include <intr/gdt.h>

#define GDT_ENTRIES 5

// the GDTR register pointing to the GDT
typedef struct {
    uint16_t limit; // number of entries in the GDT
    uint32_t base; // where the GDT is located
} __attribute__((packed)) gdtr_t;

// an entry in the GDT (a memory segment)
typedef struct {
    uint16_t limit0_15  : 16; // maximum offset allowed
    uint32_t base0_23   : 24; // where the segment begins
    uint8_t  ac         :  1; // accessed
    uint8_t  rw         :  1; // read / write allowed
    uint8_t  dc         :  1; // direction / conforming
    uint8_t  ex         :  1; // executable
    uint8_t  dt         :  1; // descriptor type
    uint8_t  dpl        :  2; // privilege (ring level)
    uint8_t  pr         :  1; // present
    uint8_t  limit16_19 :  4;
    uint8_t  reserved   :  2; // always 0 (long mode & available)
    uint8_t  sz         :  1; // size
    uint8_t  gr         :  1; // granularity
    uint8_t  base24_31  :  8;
} __attribute__((packed)) gdt_entry_t;

static gdt_entry_t gdt[GDT_ENTRIES];

static void gdt_init_entry(size_t entry, uint32_t base, uint32_t limit) {
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
    io_putstr("GDT init ... ");
    gdt_init_entry(0, 0, 0);       // null descriptor
    gdt_init_entry(1, 0, 0xFFFFF); // RING0 code segment (see gdt_flush_code_segment in gdt_asm.S)
    gdt_init_entry(2, 0, 0xFFFFF); // RING0 data segment (see gdt_flush_data_segment in gdt_asm.S)
    gdt_init_entry(3, 0, 0xFFFFF); // RING3 code segment
    gdt_init_entry(4, 0, 0xFFFFF); // RING3 data segment
    //gdt_init_entry(5, (unsigned int) tss, sizeof (tss)); // pr=1 dpl=3 ex=1 ac=1 sz=1
    gdt[0].ac = 0; gdt[0].rw = 0; gdt[0].dc = 0; gdt[0].ex = 0; gdt[0].dt = 0; gdt[0].dpl = 0; gdt[0].pr = 0; gdt[0].sz = 0; gdt[0].gr = 0;
    gdt[1].ac = 0; gdt[1].rw = 1; gdt[1].dc = 0; gdt[1].ex = 1; gdt[1].dt = 1; gdt[1].dpl = 0; gdt[1].pr = 1; gdt[1].sz = 1; gdt[1].gr = 1;
    gdt[2].ac = 0; gdt[2].rw = 1; gdt[2].dc = 0; gdt[2].ex = 0; gdt[2].dt = 1; gdt[2].dpl = 0; gdt[2].pr = 1; gdt[2].sz = 1; gdt[2].gr = 1;
    gdt[3].ac = 0; gdt[3].rw = 1; gdt[3].dc = 0; gdt[3].ex = 1; gdt[3].dt = 1; gdt[3].dpl = 3; gdt[3].pr = 1; gdt[3].sz = 1; gdt[3].gr = 1;
    gdt[4].ac = 0; gdt[4].rw = 1; gdt[4].dc = 0; gdt[4].ex = 0; gdt[4].dt = 1; gdt[4].dpl = 3; gdt[4].pr = 1; gdt[4].sz = 1; gdt[4].gr = 1;
    gdt_load();
    gdt_flush();
    io_attr(IO_GREEN);
    io_putstr("ok");
    io_attr(IO_DEFAULT);
    io_putstr(".\n");
}

uint16_t gdt_get_selector(size_t entry) {
    return (entry << 3) | gdt[entry].dpl; // selector used in IDT and segment registers
}
