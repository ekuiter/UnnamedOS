/**
 * @file
 * @addtogroup gdt
 * @{
 */

#ifndef MEM_GDT_H
#define MEM_GDT_H

#include <stdint.h>

#define GDT_ENTRIES        6 ///< number of entries in the GDT @see gdt_init
#define GDT_RING0_CODE_SEG 1 ///< kernel code segment index
#define GDT_RING0_DATA_SEG 2 ///< kernel data segment index
#define GDT_RING3_CODE_SEG 3 ///< user code segment index
#define GDT_RING3_DATA_SEG 4 ///< user data segment index
#define GDT_TASK_STATE_SEG 5 ///< task state segment index

/** An entry in the GDT. This corresponds directly to a memory segment or TSS. */
typedef struct {
    uint16_t limit0_15  : 16; ///< maximum offset allowed (byte 1-2)
    uint32_t base0_23   : 24; ///< where the segment begins (byte 1-3)
    uint8_t  ac         :  1; ///< accessed
    uint8_t  rw         :  1; ///< read / write allowed
    uint8_t  dc         :  1; ///< direction / conforming
    uint8_t  ex         :  1; ///< executable
    uint8_t  dt         :  1; ///< descriptor type
    uint8_t  dpl        :  2; /**< Descriptor privilege level. Whereas CPL is
        * the current privilege level and RPL the requested privilege level. */
    uint8_t  pr         :  1; ///< present
    uint8_t  limit16_19 :  4; ///< maximum offset allowed (last nibble)
    uint8_t  reserved   :  2; ///< always 0 (long mode & available)
    uint8_t  sz         :  1; ///< size
    uint8_t  gr         :  1; ///< granularity
    uint8_t  base24_31  :  8; ///< where the segment begins (byte 4)
} __attribute__((packed)) gdt_entry_t;

void gdt_init_entry(size_t entry, uint32_t base, uint32_t limit);
void gdt_init();
uint16_t gdt_get_selector(size_t entry);

#endif

/// @}
