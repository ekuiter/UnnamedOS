/**
 * @file
 * @addtogroup pmm
 * @{
 * Physical Memory Manager
 * 
 * The PMM allocates and frees physical page frames (blocks of 4KiB).
 * It obtains an initial memory map from GRUB.
 * @see http://wiki.osdev.org/Memory_management
 * @see http://wiki.osdev.org/Page_Frame_Allocation
 * @see http://www.lowlevel.eu/wiki/Physische_Speicherverwaltung
 * @see http://wiki.osdev.org/Detecting_Memory_(x86)
 */

#include <common.h>
#include <string.h>
#include <mem/pmm.h>
#include <boot/multiboot.h>

#define PAGE_SIZE       4096        ///< 4KB pages
#define PAGE_SHIFT      12          ///< bits to shift to get page (2^12=4096)
#define MEMORY_SIZE     0x100000000 ///< 4GB address space
#define TYPE_BITS       2           ///< number of bits per page entry
#define PAGE_NUMBER     (MEMORY_SIZE / PAGE_SIZE) ///< total number of pages
#define ENTRIES         1024        ///< number of entries in a page table
#define PAGES_PER_DWORD (32 / TYPE_BITS) ///< pages in each bitmap entry
#define PAGES_BER_BYTE  (8 / TYPE_BITS)  ///< pages per bitmap entry byte
#define TYPE_MASK       (0xFFFFFFFF >> (32 - TYPE_BITS)) ///< calc 2^TYPE_BITS-1
#define BITMAP_INIT     0x55555555  ///< 0b0101...01, use PMM_RESERVED

/// checks a bit in a given value
#define BIT_CHECK(val, bit) (((val) >> (bit)) & 1)
/// sets a bit in the memory bitmap
#define BITMAP_SET(idx)     (bitmap[(idx) / 32] |= 1 << ((idx) % 32))
/// clears a bit in the memory bitmap
#define BITMAP_CLEAR(idx)   (bitmap[(idx) / 32] &= ~(1 << ((idx) % 32)))

/** Holds information on used page frames. We could save some space here with
 * dynamic allocation if we had it. This way we use up 2MiB of memory. */
static uint32_t bitmap[PAGE_NUMBER / PAGES_PER_DWORD];
static uint32_t highest_kernel_page = 0; ///< remember the highest kernel page

// symbols defined in script.ld and main_asm.S, only the addresses matter
extern const void
        kernel_start,          ///< start of the kernel section
        kernel_end,            ///< end of the kernel section
        main_kernel_stack_end; ///< end of the main kernel stack

/// start of the main kernel stack
static void* main_kernel_stack_start = (void*) ((uintptr_t)
        &main_kernel_stack_end - STACK_SIZE + 1);

/**
 * Sets a bitmap entry to a value.
 * @param idx   index into the bitmap
 * @param value value to set the entry to, can be 0 to TYPE_MASK
 */
static void pmm_bitmap_set(uint32_t idx, uint32_t value) {
    idx *= TYPE_BITS; // make this flexible so that more types can be added
    for (int i = 0; i < TYPE_BITS; i++)
        if (BIT_CHECK(value, i))
            BITMAP_SET(idx + i);
        else
            BITMAP_CLEAR(idx + i);
}

/**
 * Returns a bitmap entry.
 * @param idx index into the bitmap
 * @return value of the bitmap entry, can be 9 to TYPE_MASK
 */
static uint32_t pmm_bitmap_get(uint32_t idx) {
    idx *= TYPE_BITS;
    return (bitmap[idx / 32] >> (idx % 32)) & TYPE_MASK;
}

/// Marks all kernel memory as used.
static void pmm_use_kernel_memory() {
    logln("PMM", "Kernel memory:");
    logln("PMM", "   kernel=%08x-%08x", &kernel_start,    &kernel_end);
    logln("PMM", "    stack=%08x-%08x", main_kernel_stack_start, &main_kernel_stack_end);
    pmm_use((void*) &kernel_start, (uintptr_t) &kernel_end -
            (uintptr_t) &kernel_start + 1, PMM_KERNEL, "kernel");
}

/// Initializes the PMM.
void pmm_init() {
    print("PMM init ... ");
    /// First assumes the whole memory is used, GRUB tells us about free memory.
    memset(bitmap, BITMAP_INIT, sizeof(bitmap));
    if (!multiboot_free_memory()) {
        println("%4afail%a. Memory map not found.");
        return;
    }
    /// Prevents that we allocate or dereference the null pointer or BIOS data
    /// by pmm_use()'ing the first page table.
    // pmm_use(0, MULTIBOOT_FIRST_PAGE_TABLE, PMM_RESERVED, "VM86 memory");
    // We could do it like that, but a direct memset proves to be faster:
    logln("PMM", "Use the first megabyte for VM86");
    memset(bitmap, BITMAP_INIT, ENTRIES / PAGES_BER_BYTE);
    pmm_use_kernel_memory(); // Maps the actual kernel code and data.
    /// Copies the multiboot structures somewhere into the kernel so
    /// we can overwrite lower memory in VM86 mode later. Because our kernel
    /// starts at 4 MiB (the 2nd page table) we reserved the first page table
    /// above so that the multiboot structures lie directly after the kernel.
    multiboot_copy_memory();
    /// After the structures were copied from lower memory, frees
    /// 0x100000-0x3FFFFF to not waste too much memory.
    pmm_use((void*) MULTIBOOT_LOWER_MEMORY,
            MULTIBOOT_FIRST_PAGE_TABLE - MULTIBOOT_LOWER_MEMORY, PMM_UNUSED, 0);    
    println("%2aok%a.");
}

/**
 * Returns to which page a given memory address belongs.
 * @param ptr    a physical or virtual address
 * @param offset number of bytes to add to ptr
 * @return the page index that contains the address ptr
 */
uint32_t pmm_get_page(void* ptr, uint32_t offset) {
    return ((uintptr_t) ptr + offset) >> PAGE_SHIFT;
}

/**
 * Returns a memory address belonging to a given page.
 * @param page   a page index
 * @param offset number of bytes to add to the start of the page
 * @return the first address of the page (if given, the offset is added)
 */
void* pmm_get_address(uint32_t page, uint32_t offset) {
    return (void*) ((page << PAGE_SHIFT) + offset);
}

/**
 * Marks page frames for a given memory range as used or unused.
 * @param ptr   the physical start address of the memory range
 * @param len   the length of the memory range in bytes
 * @param flags whether to allocate or free page frames
 * @param tag   a short string for the debug log
 */
void pmm_use(void* ptr, size_t len, pmm_flags_t flags, char* tag) {
    if (len == 0) return;
    uint32_t start_page = pmm_get_page(ptr, 0), end_page = pmm_get_page(ptr, len - 1);
    log("PMM", "%s %08x-%08x (page %05x-%05x)", flags == PMM_UNUSED ? "Free" : "Use ",
            ptr, ptr + len - 1, start_page, end_page);
    if (tag)
        log(0, " for %s", tag);
    logln(0, "");
    for (int i = start_page; i <= end_page; i++)
        pmm_bitmap_set(i, flags); // mark pages as used in the bitmap
    if (flags == PMM_KERNEL && end_page > highest_kernel_page)
        highest_kernel_page = end_page;
}

/**
 * Finds free page frames.
 * @param len requested number of consecutive free bytes
 * @return the physical address of a suitable free memory range
 */
static void* pmm_find_free(size_t len) {
    if (len == 0) return 0;
    uint32_t pages = len / PAGE_SIZE + (len % PAGE_SIZE ? 1 : 0), // "round up"
            free_pages = 0;
    for (int i = 0; i < PAGE_NUMBER; i++) { // makeshift first-fit linear search
        if (pmm_bitmap_get(i) == PMM_UNUSED)
            free_pages++;
        else
            free_pages = 0;
        if (free_pages >= pages) // if we found enough in-a-row pages,
            // return a pointer to the first page's beginning
            return pmm_get_address(i - free_pages + 1, 0);
    }
    println("%4aPMM: Not enough memory%a");
    return 0;
}

/**
 * Allocates page frames. 
 * @param len   requested number of consecutive free bytes 
 * @param flags whether to allocate for kernel or user space
 * @return the physical start address of the allocated memory range
 */
void* pmm_alloc(size_t len, pmm_flags_t flags) {   
    void* ptr = pmm_find_free(len); // find some free pages in a row
    if (!ptr)
        return 0;
    pmm_use(ptr, len, flags, "pmm_alloc"); // mark the pages as used
    return ptr;
}

/**
 * Frees page frames.
 * @param ptr the physical start address of the memory range
 * @param len the length of the memory range in bytes
 */
void pmm_free(void* ptr, size_t len) {
    if (len == 0) return;
    pmm_use(ptr, len, PMM_UNUSED, 0);
}

/**
 * Returns whether a page frame is used or unused.
 * @param ptr the physical address of the page frame to check
 * @return information on who uses the page
 */
pmm_flags_t pmm_check(void* ptr) {
    return pmm_bitmap_get(pmm_get_page(ptr, 0));
}

/**
 * Dumps information on page frames for a given memory range. The dump is logged.
 * @param ptr the physical start address of the memory range
 * @param len the length of the memory range in bytes
 */
void pmm_dump(void* ptr, size_t len) {
    if (len == 0) return;
    uint32_t start_page = pmm_get_page(ptr, 0), end_page = pmm_get_page(ptr, len - 1);
    log("PMM", "Memory bitmap from page %05x to %05x:", start_page, end_page);
    for (int i = start_page; i <= end_page; i++) {
        if ((i - start_page) % 64 == 0) { // line break every 64*PAGE_SIZE bytes
            uint32_t kilobytes = i * PAGE_SIZE / 1024;
            logln(0, ""), log("PMM", "[%7d%cB] ",
                    kilobytes % 1024 == 0 ? kilobytes / 1024 : kilobytes,
                    kilobytes % 1024 == 0 ? 'M' : 'K');
        }
        log(0, "%x", pmm_bitmap_get(i));
    }
    logln(0, "");
}

/**
 * Returns the highest page used by the kernel.
 * @return highest kernel page
 */
uint32_t pmm_get_highest_kernel_page() {
    return highest_kernel_page;
}

/// @}
