/**
 * @file
 * @addtogroup tss
 * @{
 */

#ifndef TASKS_TSS_H
#define TASKS_TSS_H

#include <stdint.h>
#include <mem/gdt.h>

void tss_init(gdt_entry_t* gdt);
void tss_set_stack(uint32_t stack_pointer);
void tss_load();

#endif

/// @}
