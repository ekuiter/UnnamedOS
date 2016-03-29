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
#define PAGES_PER_DWORD (32 / TYPE_BITS) // pages in each bitmap entry
#define TYPE_MASK       (0xFFFFFFFF >> (32 - TYPE_BITS)) // calc 2^TYPE_BITS-1
#define BITMAP_INIT     0x55555555  // 0b0101...01, use PMM_RESERVED

#define BIT_CHECK(val, bit) (((val) >> (bit)) & 1)
#define BITMAP_SET(idx)     (bitmap[(idx) / 32] |= 1 << ((idx) % 32))
#define BITMAP_CLEAR(idx)   (bitmap[(idx) / 32] &= ~(1 << ((idx) % 32)))

static uint32_t bitmap[PAGE_NUMBER / PAGES_PER_DWORD]; // saves page entries
static uint32_t last_kernel_page = 0; // remember the highest kernel page
static uint32_t allocations = 0; // track the number of pmm_alloc calls

// symbols defined in script.ld and main_asm.S, only the addresses matter
extern const void kernel_start, kernel_end, multiboot_start, multiboot_end,
        text_start, text_end, data_start, data_end, rodata_start, rodata_end,
        bss_start, bss_end, main_kernel_stack;

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
    logln("PMM", "    stack=%08x-%08x", &main_kernel_stack - STACK_SIZE, &main_kernel_stack);
    pmm_use((void*) &kernel_start, &kernel_end - &kernel_start + 1, PMM_KERNEL, "kernel");
}

void pmm_init() {
    print("PMM init ... ");
    memset(bitmap, BITMAP_INIT, sizeof(bitmap)); // assume the whole memory is
    if (!multiboot_init_memory()) { // used, GRUB tells us about free memory
        println("%4afail%a. Memory map not found.");
        return;
    }
    pmm_use(0, 1, PMM_RESERVED, "null pointer"); // prevent that we allocate 0
    pmm_use_kernel_memory();
    io_init();
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
    allocations++;
    return ptr;
}

void pmm_free(void* ptr, size_t len) {
    if (len == 0) return;
    pmm_use(ptr, len, PMM_UNUSED, 0);
    allocations--;
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

uint32_t pmm_get_allocations() {
    return allocations;
}

void pmm_reset_allocations() {
    allocations = 0;
}

uint32_t pmm_get_last_kernel_page() {
    return last_kernel_page;
}
