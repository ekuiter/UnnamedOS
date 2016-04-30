/**
 * @file
 * @addtogroup mmu
 * @{
 * Memory Management Unit
 * 
 * The MMU deals with technical details of paging and the Translation Lookaside
 * Buffer.
 * @see http://wiki.osdev.org/Memory_Management_Unit
 * @see http://www.lowlevel.eu/wiki/Steuerregister
 * @see http://wiki.osdev.org/TLB
 */

#include <common.h>
#include <interrupts/isr.h>
#include <mem/mmu.h>

/// page fault error information
typedef union {
    struct {
        uint8_t pr       : 1, ///< present flag
                rw       : 1, ///< write flag
                user     : 1, ///< user flag
                reserved : 1, ///< reserved write flag
                _if      : 1; ///< instruction flag fetch
    } __attribute__((packed)) bits; ///< bit field
    uint32_t dword;           ///< useful for casting
} mmu_page_fault_error_t;

/**
 * Loads a page directory into the CR3 register.
 * @param page_directory physical address of a page directory
 */
void mmu_load_page_directory(page_directory_t* page_directory) {
    // Load the directory pointer into control register 3. (Also flushes the TLB!)
    asm volatile("mov %0, %%cr3" : : "r" (page_directory));
}

/**
 * Loads a page directory and enables paging.
 * @param page_directory physical address of a page directory (or 0 if paging
 *                       should be disabled)
 */
void mmu_enable_paging(page_directory_t* page_directory) {
    if (page_directory) {
        mmu_load_page_directory(page_directory); // first load the initial directory
        // then set bit 31 (the paging flag) in control register 0 to enable paging    
        asm volatile("mov %cr0, %eax; or $0x80000000, %eax; mov %eax, %cr0");
    } else
        asm volatile("mov %cr0, %eax; and $0x7FFFFFFF, %eax; mov %eax, %cr0");
}

/**
 * Returns whether paging is enabled or disabled.
 * @return paging flag
 */
uint8_t mmu_get_paging() {
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r" (cr0));
    return cr0 >> 31;
}

/**
 * Flushes the Translation Lookaside Buffer for the given page. This removes
 * any cached mapping from virtual to physical addresses for this page, which
 * is necessary when page directories are modified while they are in use.
 * @param vaddr the virtual address of a page to flush
 */
void mmu_flush_tlb(void* vaddr) {
    // invlpg invalidates a page table entry in the TLB to notify it of a change.
    // We pass the address as a "r"egister input. "memory" ensures GCC does not
    asm volatile("invlpg (%0)" : : "r" (vaddr) : "memory"); // do funny things.
}

/**
 * Handles page faults.
 * @param cpu the CPU state
 * @return a (possibly altered) CPU state
 */
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

/// Initializes the MMU.
void mmu_init() {
    isr_register_handler(ISR_EXCEPTION(0x0E), mmu_handle_page_fault);
}

/// @}
