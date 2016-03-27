/*
 * Memory Management Unit - technical details of paging and the TLB
 * 
 * http://wiki.osdev.org/Memory_Management_Unit
 * http://www.lowlevel.eu/wiki/Steuerregister
 * http://wiki.osdev.org/TLB
 */

#include <common.h>
#include <interrupts/isr.h>
#include <mem/mmu.h>

typedef union {
    struct {
        uint8_t pr : 1, rw : 1, user : 1, reserved : 1, _if : 1;
    } __attribute__((packed)) bits;
    uint32_t dword;
} mmu_page_fault_error_t;

// page_directory HAS to be a physical address!
void mmu_load_page_directory(page_directory_t* page_directory) {
    // Load the directory pointer into control register 3. (Also flushes the TLB!)
    asm volatile("mov %0, %%cr3" : : "r" (page_directory));
}

// page_directory HAS to be a physical address!
void mmu_enable_paging(page_directory_t* page_directory) {
    if (page_directory) {
        mmu_load_page_directory(page_directory); // first load the initial directory
        // then set bit 31 (the paging flag) in control register 0 to enable paging    
        asm volatile("mov %cr0, %eax; or $0x80000000, %eax; mov %eax, %cr0");
    } else
        asm volatile("mov %cr0, %eax; and $0x7FFFFFFF, %eax; mov %eax, %cr0");
}

uint8_t mmu_get_paging() {
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r" (cr0));
    return cr0 >> 31;
}

// vaddr HAS to be a virtual address!
void mmu_flush_tlb(void* vaddr) { // (= Translation Lookaside Buffer)
    // invlpg invalidates a page table entry in the TLB to notify it of a change.
    // We pass the address as a "r"egister input. "memory" ensures GCC does not
    asm volatile("invlpg (%0)" : : "r" (vaddr) : "memory"); // do funny things.
}

static cpu_state_t* mmu_handle_page_fault(cpu_state_t* cpu) {
    uint32_t fault_address; // the virtual address that caused the page fault
    mmu_page_fault_error_t error = (mmu_page_fault_error_t) cpu->error;
    asm volatile("mov %%cr2, %0" : "=r" (fault_address));
    println("%4apage fault caused by the virtual address %08x\n"
            "(%s while %s %s%s%s)%a", fault_address,
            error.bits.pr ? "protection violation" : "non-present page",
            error.bits.rw ? "writing" : "reading",
            error.bits.user ? "in user space" : "in the kernel",
            error.bits.reserved ? ", reserved write" : "",
            error.bits._if ? ", instruction fetch" : "");
    panic("%4aEX%02x (EIP=%08x)", cpu->intr, cpu->eip);
    return cpu;
}

void mmu_init() {
    isr_register_handler(ISR_EXCEPTION(0x0E), mmu_handle_page_fault);
}
