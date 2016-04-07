/*
 * Physical Memory Manager - keeps track of the physical memory pages
 * 
 * http://wiki.osdev.org/Memory_management
 * http://wiki.osdev.org/Page_Frame_Allocation
 * http://www.lowlevel.eu/wiki/Physische_Speicherverwaltung
 * http://wiki.osdev.org/Detecting_Memory_(x86)
 */

#include <common.h>
#include <string.h>
#include <mem/pmm.h>
#include <boot/multiboot.h>

#define PAGE_SIZE       4096        // 4KB pages
#define PAGE_SHIFT      12          // bits to shift to get page (2^12=4096)
#define MEMORY_SIZE     0x100000000 // 4GB address space
#define TYPE_BITS       2           // number of bits per page entry
#define PAGE_NUMBER     (MEMORY_SIZE / PAGE_SIZE) // total number of pages
#define ENTRIES         1024        // number of entries in a page table
#define PAGES_PER_DWORD (32 / TYPE_BITS) // pages in each bitmap entry
#define PAGES_BER_BYTE  (8 / TYPE_BITS)  // pages per bitmap entry byte
#define TYPE_MASK       (0xFFFFFFFF >> (32 - TYPE_BITS)) // calc 2^TYPE_BITS-1
#define BITMAP_INIT     0x55555555  // 0b0101...01, use PMM_RESERVED

#define BIT_CHECK(val, bit) (((val) >> (bit)) & 1)
#define BITMAP_SET(idx)     (bitmap[(idx) / 32] |= 1 << ((idx) % 32))
#define BITMAP_CLEAR(idx)   (bitmap[(idx) / 32] &= ~(1 << ((idx) % 32)))

static uint32_t bitmap[PAGE_NUMBER / PAGES_PER_DWORD]; // saves page entries
static uint32_t last_kernel_page = 0; // remember the highest kernel page

// symbols defined in script.ld and main_asm.S, only the addresses matter
extern const void kernel_start, kernel_end, multiboot_start, multiboot_end,
        text_start, text_end, data_start, data_end, rodata_start, rodata_end,
        bss_start, bss_end, main_kernel_stack_end;
static void* main_kernel_stack_start = (void*) ((uintptr_t)
        &main_kernel_stack_end - STACK_SIZE + 1);

static void pmm_bitmap_set(uint32_t idx, uint32_t value) {
    idx *= TYPE_BITS; // make this flexible so that more types can be added
    for (int i = 0; i < TYPE_BITS; i++)
        if (BIT_CHECK(value, i))
            BITMAP_SET(idx + i);
        else
            BITMAP_CLEAR(idx + i);
}

static uint32_t pmm_bitmap_get(uint32_t idx) {
    idx *= TYPE_BITS;
    return (bitmap[idx / 32] >> (idx % 32)) & TYPE_MASK;
}

static void pmm_use_kernel_memory() {
    logln("PMM", "Kernel memory:");
    logln("PMM", "   kernel=%08x-%08x", &kernel_start,    &kernel_end);
    logln("PMM", "multiboot=%08x-%08x", &multiboot_start, &multiboot_end);
    logln("PMM", "     text=%08x-%08x", &text_start,      &text_end);
    logln("PMM", "     data=%08x-%08x", &data_start,      &data_end);
    logln("PMM", "   rodata=%08x-%08x", &rodata_start,    &rodata_end);
    logln("PMM", "      bss=%08x-%08x", &bss_start,       &bss_end);
    logln("PMM", "    stack=%08x-%08x", main_kernel_stack_start, &main_kernel_stack_end);
    pmm_use((void*) &kernel_start, (uintptr_t) &kernel_end -
            (uintptr_t) &kernel_start + 1, PMM_KERNEL, "kernel");
}

void pmm_init() {
    print("PMM init ... ");
    // Assume the whole memory is used, GRUB will tell us about free memory.
    memset(bitmap, BITMAP_INIT, sizeof(bitmap));
    if (!multiboot_free_memory()) {
        println("%4afail%a. Memory map not found.");
        return;
    }
    // prevent that we allocate or dereference the null pointer or BIOS data
    // by pmm_use'ing the first page table:
    // pmm_use(0, MULTIBOOT_FIRST_PAGE_TABLE, PMM_RESERVED, "VM86 memory");
    // We could do it like that, but a direct memset proves to be faster:
    logln("PMM", "Use the first megabyte for VM86");
    memset(bitmap, BITMAP_INIT, ENTRIES / PAGES_BER_BYTE);
    pmm_use_kernel_memory(); // the actual kernel code and data
    // We need to copy the multiboot structures somewhere into the kernel so
    // we can overwrite lower memory in VM86 mode later. Because our kernel
    // starts at 4 MiB (the 2nd page table) we reserved the first page table
    multiboot_copy_memory(); // above so that multiboot lies beyond the kernel.
    // Now that we have copied the everything from lower memory, we can free
    // 0x100000-0x3FFFFF to not waste too much memory. (This memory is likely to
    pmm_use((void*) MULTIBOOT_LOWER_MEMORY, // be used for paging by vmm_init.)
            MULTIBOOT_FIRST_PAGE_TABLE - MULTIBOOT_LOWER_MEMORY, PMM_UNUSED, 0);    
    println("%2aok%a.");
}

uint32_t pmm_get_page(void* ptr, uint32_t offset) {
    return ((uintptr_t) ptr + offset) >> PAGE_SHIFT;
}

void* pmm_get_address(uint32_t page, uint32_t offset) {
    return (void*) ((page << PAGE_SHIFT) + offset);
}

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
    if (flags == PMM_KERNEL && end_page > last_kernel_page)
        last_kernel_page = end_page;
}

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

void* pmm_alloc(size_t len, pmm_flags_t flags) {   
    void* ptr = pmm_find_free(len); // find some free pages in a row
    if (!ptr)
        return 0;
    pmm_use(ptr, len, flags, "pmm_alloc"); // mark the pages as used
    return ptr;
}

void pmm_free(void* ptr, size_t len) {
    if (len == 0) return;
    pmm_use(ptr, len, PMM_UNUSED, 0);
}

pmm_flags_t pmm_check(void* ptr) {
    return pmm_bitmap_get(pmm_get_page(ptr, 0));
}

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

uint32_t pmm_get_last_kernel_page() {
    return last_kernel_page;
}
