#ifndef MEM_GDT_H
#define MEM_GDT_H

#include <stdint.h>

#define GDT_ENTRIES        6
#define GDT_RING0_CODE_SEG 1
#define GDT_RING0_DATA_SEG 2
#define GDT_RING3_CODE_SEG 3
#define GDT_RING3_DATA_SEG 4
#define GDT_TASK_STATE_SEG 5

// an entry in the GDT (a memory segment)
typedef struct {
    uint16_t limit0_15  : 16; // maximum offset allowed
    uint32_t base0_23   : 24; // where the segment begins
    uint8_t  ac         :  1; // accessed
    uint8_t  rw         :  1; // read / write allowed
    uint8_t  dc         :  1; // direction / conforming
    uint8_t  ex         :  1; // executable
    uint8_t  dt         :  1; // descriptor type
    uint8_t  dpl        :  2; // desc. priv. level (CPL = current, RPL = requested)
    uint8_t  pr         :  1; // present
    uint8_t  limit16_19 :  4;
    uint8_t  reserved   :  2; // always 0 (long mode & available)
    uint8_t  sz         :  1; // size
    uint8_t  gr         :  1; // granularity
    uint8_t  base24_31  :  8;
} __attribute__((packed)) gdt_entry_t;

void gdt_init_entry(size_t entry, uint32_t base, uint32_t limit);
void gdt_init();
uint16_t gdt_get_selector(size_t entry);

#endif
