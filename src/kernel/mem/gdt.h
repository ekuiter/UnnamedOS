#ifndef MEM_GDT_H
#define MEM_GDT_H

#include <stdint.h>

void gdt_init();
uint16_t gdt_get_selector(size_t entry);

#endif
