/**
 * @file
 * @addtogroup idt
 * @{
 */

#ifndef INTERRUPTS_IDT_H
#define INTERRUPTS_IDT_H

/** Number of entries in the IDT. Only the entries 0x00-0x30 are actually used. */
#define IDT_ENTRIES 256

void idt_init();

#endif

/// @}
