/**
 * @file
 * @addtogroup tss
 * @{
 * Task State Segment
 * 
 * The TSS stores information for switching from user to kernel space.
 * @see http://wiki.osdev.org/TSS
 * @see https://en.wikipedia.org/wiki/Task_state_segment
 * @see http://www.lowlevel.eu/wiki/Task_State_Segment
 * @see http://wiki.osdev.org/Getting_to_Ring_3
 * @see http://www.jamesmolloy.co.uk/tutorial_html/10.-User%20Mode.html
 */

#include <common.h>
#include <tasks/tss.h>

/// the task state segment
typedef struct {
    uint32_t : 32,      ///< unused by this implementation
            esp0 : 32,  ///< the stack pointer to load when entering the kernel
            ss0 : 16,   ///< the stack segment to load when entering the kernel
                        ///< (always the kernel data segment selector)
            : 16, : 32, : 32, : 32, : 32, : 32, : 32, : 32, : 32, : 32, : 32, : 32,
            : 32, : 32, : 32, : 32, : 32, : 32, : 32, : 32, : 32, : 32, : 32, : 16,
            iopb : 16; ///< @todo maybe use this for I/O permissions
} __attribute__((packed)) tss_t;

static tss_t tss; ///< the TSS, for software multitasking we only need one

/**
 * Initializes the TSS.
 * @param gdt pointer to the GDT
 */
void tss_init(gdt_entry_t* gdt) {
    /// Creates a TSS descriptor in the GDT.
    gdt_init_entry(GDT_TASK_STATE_SEG, (uint32_t) &tss, sizeof(tss));
    gdt[5].ac  = 1; gdt[5].rw = 0; gdt[5].dc = 0; gdt[5].ex = 1; gdt[5].dt = 0;
    gdt[5].dpl = 3; gdt[5].pr = 1; gdt[5].sz = 1; gdt[5].gr = 0;
    /// Sets the TSS's stack segment to the kernel stack segment.
    tss.ss0 = gdt_get_selector(GDT_RING0_DATA_SEG);
}

/**
 * Sets the TSS's kernel stack which is used to handle interrupts.
 * @param stack_pointer the kernel stack pointer
 */
void tss_set_stack(uint32_t stack_pointer) {
    tss.esp0 = stack_pointer;
}

/// Loads the TSS into the TR register.
void tss_load() {
    /// Loads the TR (= Task Register) with the TSS's GDT selector.
    uint16_t tss_selector = gdt_get_selector(GDT_TASK_STATE_SEG);
    asm volatile("ltr %0" : : "a" (tss_selector));
}

/// @}
