/*
 * Task State Segment - stores information for switching to kernel space
 *
 * http://wiki.osdev.org/TSS
 * https://en.wikipedia.org/wiki/Task_state_segment
 * http://www.lowlevel.eu/wiki/Task_State_Segment
 * http://wiki.osdev.org/Getting_to_Ring_3
 * http://www.jamesmolloy.co.uk/tutorial_html/10.-User%20Mode.html
 */

#include <common.h>
#include <tasks/tss.h>

// the task state segment
typedef struct {
    uint32_t : 32,
            esp0 : 32,  // the stack pointer and
            ss0 : 16,   // the stack segment to load when entering the kernel
            : 16, : 32, // (always the kernel data segment selector)
            : 32, : 32, : 32, : 32, : 32, : 32, : 32, : 32, : 32, : 32, : 32,
            : 32, : 32, : 32, : 32, : 32, : 32, : 32, : 32, : 32, : 32, : 16,
            iopb : 16; // TODO: maybe use this for I/O permissions
} __attribute__((packed)) tss_t;

static tss_t tss; // our TSS, for software multitasking we only need one

void tss_init(gdt_entry_t* gdt) {
    gdt_init_entry(GDT_TASK_STATE_SEG, (uint32_t) &tss, sizeof(tss)); // TSS descriptor
    gdt[5].ac  = 1; gdt[5].rw = 0; gdt[5].dc = 0; gdt[5].ex = 1; gdt[5].dt = 0;
    gdt[5].dpl = 3; gdt[5].pr = 1; gdt[5].sz = 1; gdt[5].gr = 0;
    tss.ss0 = gdt_get_selector(GDT_RING0_DATA_SEG); // kernel stack segment
}

void tss_set_stack(uint32_t stack_pointer) {
    tss.esp0 = stack_pointer; // kernel stack to use while handling interrupts
}

void tss_load() {
    // load the TR (= Task Register) with the TSS's GDT selector (see GDT/IDT)
    uint16_t tss_selector = gdt_get_selector(GDT_TASK_STATE_SEG);
    asm volatile("ltr %0" : : "a" (tss_selector));
}
