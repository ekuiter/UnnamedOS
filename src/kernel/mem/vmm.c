/*
 * Virtual Memory Manager - manages paging and memory contexts
 * 
 * http://wiki.osdev.org/Paging
 * http://www.lowlevel.eu/wiki/Paging
 * http://www.rohitab.com/discuss/topic/31139-tutorial-paging-memory-mapping-with-a-recursive-page-directory/
 */

#include <common.h>
#include <string.h>
#include <mem/vmm.h>
#include <mem/mmu.h>

#define ENTRIES 1024 // number of entries in page directories and tables
// The number of bytes per page directory / table "happens" to equal the size
#define PAGE_SIZE (ENTRIES * sizeof(page_directory_entry_t)) // of a page.
#define MEMORY_SIZE     0x100000000 // 4GB address space
#define PAGE_NUMBER     (MEMORY_SIZE / PAGE_SIZE) // total number of pages

typedef union {
    struct {
        uint16_t page_offset : 12; // location in the page
        uint16_t page        : 10; // index in the page table
        uint16_t page_table  : 10; // index in the page directory
    } __attribute__((packed)) bits;
    void* ptr;
} vmm_virtual_address_t;

static page_directory_t* page_directory; // the current page directory

page_directory_t* vmm_get_page_directory() {
    return page_directory;
}

void vmm_set_page_directory(page_directory_t* _page_directory) {
    page_directory = _page_directory;
}

static page_table_t* vmm_create_page_table() {
    return memset(pmm_alloc(PAGE_SIZE, PMM_KERNEL), 0, PAGE_SIZE);
}

static void vmm_destroy_page_table(uint16_t page_table) {
    // free the page table memory (using its physical address!)
    pmm_free(pmm_get_address(page_directory[page_table].pt, 0), PAGE_SIZE);
    // tell the page directory that this page table was deleted
    memset(page_directory + page_table, 0, sizeof(page_directory_entry_t));
}

page_directory_t* vmm_create_page_directory() {
    page_directory_t* page_directory =
            memset(pmm_alloc(PAGE_SIZE, PMM_KERNEL), 0, PAGE_SIZE);
    logln("VMM", "Creating page directory at %08x", page_directory);
    // for a detailed explanation on why we do this, see vmm_init
    page_directory_entry_t dir_entry = {
        .pr = 1, .rw = 0, .user = 0, .pt = pmm_get_page(page_directory, 0)
    };
    page_directory[ENTRIES - 1] = dir_entry;
    return page_directory; // this is a physical address for a page directory
}

void vmm_destroy_page_directory() {
    // if there are any page tables associated with this directory, delete them
    for (int i = 0; i < ENTRIES; i++)
        if (page_directory[i].pr)
            vmm_destroy_page_table(i);
    // We don't need to free the page directory itself, because we mapped it as
    // a page table, so it was the last page table removed above.
}

uint8_t vmm_map(void* _vaddr, void* paddr, vmm_flags_t flags) {
    vmm_virtual_address_t vaddr = (vmm_virtual_address_t) _vaddr;
    // find the page directory entry associated with this page
    page_directory_entry_t* dir_entry = page_directory + vaddr.bits.page_table;
    if (!dir_entry->pr) { // if the table doesn't exist yet, create it
        // We assume the table's pages to be writable and in userspace for now,
        // this is overridden by individual pages below.
        dir_entry->pr = dir_entry->rw = dir_entry->user = 1;
        dir_entry->pt = pmm_get_page(vmm_create_page_table(), 0);
    }
    page_table_entry_t* tab_entry;
    // If paging is already enabled and we access the virtually mapped page
    // directory, we need to calculate the page table pointer in a different way
    if (page_directory == VMM_PAGEDIR) // (see vmm_init for details).
        tab_entry = VMM_PAGETAB(vaddr.bits.page_table) + vaddr.bits.page;
    else
        tab_entry = (page_table_t*) pmm_get_address(dir_entry->pt, 0) +
                vaddr.bits.page;
    if (tab_entry->pr) {
        println("%4aVMM: %08x is already mapped%a", vaddr);
        return 0; // if we already mapped this page, cancel and return
    }
    tab_entry->pr = 1;
    tab_entry->rw = flags == VMM_READWRITE;
    tab_entry->user = flags != VMM_KERNEL;
    tab_entry->page = pmm_get_page(paddr, 0); // otherwise, map the page
    if (page_directory == VMM_PAGEDIR) // if we changed the currently used
        mmu_flush_tlb(_vaddr); // directory, flush the TLB to apply changes
    return 1;
}

void vmm_unmap(void* _vaddr) {
    vmm_virtual_address_t vaddr = (vmm_virtual_address_t) _vaddr;
    page_directory_entry_t* dir_entry = page_directory + vaddr.bits.page_table;
    if (!dir_entry->pr) // if the table doesn't exist yet, do nothing
        return;
    page_table_t* page_table;
    if (page_directory == VMM_PAGEDIR)
        page_table = VMM_PAGETAB(vaddr.bits.page_table);
    else
        page_table = (page_table_t*) pmm_get_address(dir_entry->pt, 0);
    page_table_entry_t* tab_entry = page_table + vaddr.bits.page;
    if (!tab_entry->pr)
        return; // if the page was not yet mapped, do nothing
    memset(tab_entry, 0, sizeof(page_table_entry_t)); // remove the mapping
    int i;
    for (i = 0; i < ENTRIES; i++) // search the page table
        if (page_table[i].pr) // for other present entries
            break;
    if (i == ENTRIES - 1) // if there are none, we can safely free the table
        vmm_destroy_page_table(vaddr.bits.page_table);
    if (page_directory == VMM_PAGEDIR)
        mmu_flush_tlb(_vaddr);
}

static void vmm_map_range_detailed(void* vaddr, void* paddr, size_t len,
        vmm_flags_t flags, uint8_t map) {
    uint32_t virtual_page = pmm_get_page(vaddr, 0),
            physical_page = pmm_get_page(paddr, 0),
            pages         = pmm_get_page(vaddr, len - 1) - virtual_page + 1;
    logln("VMM", map ? "Map   virtual %08x-%08x (page %05x-%05x) "
            "to physical %08x-%08x (page %05x-%05x)" :
            "Unmap virtual %08x-%08x (page %05x-%05x)",
            vaddr, vaddr + len - 1, virtual_page,  virtual_page  + pages - 1,
            paddr, paddr + len - 1, physical_page, physical_page + pages - 1);
    if (map)
        for (int i = 0; i < pages; i++)
            vmm_map(pmm_get_address(virtual_page  + i, 0),
                    pmm_get_address(physical_page + i, 0), flags);
    else
        for (int i = 0; i < pages; i++)
            vmm_unmap(pmm_get_address(virtual_page + i, 0));
}

void vmm_map_range(void* vaddr, void* paddr, size_t len, vmm_flags_t flags) {
    if (len == 0) return;
    vmm_map_range_detailed(vaddr, paddr, len, flags, 1);
}

void vmm_unmap_range(void* vaddr, size_t len) {
    if (len == 0) return;
    vmm_map_range_detailed(vaddr, 0, len, 0, 0);
}

void* vmm_get_physical_address(void* _vaddr) {
    vmm_virtual_address_t vaddr = (vmm_virtual_address_t) _vaddr;
    page_directory_entry_t* dir_entry = page_directory + vaddr.bits.page_table;
    if (!dir_entry->pr)
        return 0; // this virtual address is not currently mapped
    page_table_entry_t* tab_entry;
    if (page_directory == VMM_PAGEDIR)
        tab_entry = VMM_PAGETAB(vaddr.bits.page_table) + vaddr.bits.page;
    else
        tab_entry = (page_table_t*) pmm_get_address(dir_entry->pt, 0) +
                vaddr.bits.page;
    if (!tab_entry->pr)
        return 0;
    return pmm_get_address(tab_entry->page, vaddr.bits.page_offset);
}

void vmm_dump() {
    logln("VMM", "Page directory at %08x:", page_directory);
    for (int i = 0; i < ENTRIES; i += 16) {
        page_directory_entry_t* p = page_directory + i;
        logln("VMM", "[%04d] %08x %08x %08x %08x %08x %08x %08x %08x "
                "%08x %08x %08x %08x %08x %08x %08x %08x", i,
            p[0], p[1], p[2],  p[3],  p[4],  p[5],  p[6],  p[7],
            p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
    }
}

void vmm_use(void* ptr, size_t len, vmm_flags_t flags, char* tag) {
    pmm_use(ptr, len, flags == VMM_KERNEL ? PMM_KERNEL : PMM_USER, tag);
    vmm_map_range(ptr, ptr, len, flags);
}

void* vmm_alloc(size_t len, vmm_flags_t flags) {
    void* addr = pmm_alloc(len, flags == VMM_KERNEL ? PMM_KERNEL : PMM_USER);
    vmm_map_range(addr, addr, len, flags);
    return addr;
}

void vmm_free(void* ptr, size_t len) {
    vmm_unmap_range(ptr, len);
    pmm_free(ptr, len);
}

void vmm_init() {
    print("VMM init ... ");
    mmu_init();
    vmm_set_page_directory(vmm_create_page_directory());
    // identity map all up to now used pages (but do not map the last 4MiB that
    for (int i = 0; i < PAGE_NUMBER - ENTRIES; i++) { // contain the directory)
        void* addr = pmm_get_address(i, 0);
        if (pmm_check(addr) != PMM_UNUSED && pmm_check(addr) != PMM_RESERVED)
            vmm_map(addr, addr, VMM_KERNEL); // only accessible to the kernel
    }
    // Above we let the last directory entry point to itself so we can use it
    // as a page table to be able to dynamically (un)map and virtual addresses.
    // This allows us to refer to the page directory (wherever it is in physical
    // memory) with the virtual address 0xFFFFF000 as soon as paging is enabled:
    // 0xFFFFF000 = 0b 1111111111 1111111111 000000000000
    //                 dir_idx    tab_idx    page_offset, so 0xFFFFF000 refers
    // to the last directory entry (which is the page directory itself) and then
    // the last table entry which is the directory again, in a recursive manner.
    // So 0xFFFFF000 points to the page directory and is one page (4096 or
    // 0x1000 bytes) long, so it lies at 0xFFFFF000-0xFFFFFFFF (at the very end).
    mmu_enable_paging(page_directory);
    // Now we are working with virtual addresses, so to access our directory we
    // use 0xFFFFF000 as explained above. From now on all addresses are virtual
    // addresses, but this is transparent because we identity-mapped the kernel!
    vmm_set_page_directory(VMM_PAGEDIR);
    // Note that in this way, we can access all page tables as well. Take, for
    // example, the first page table: Because we mapped the last directory entry
    // to the directory itself, the last 4MB of our address space (0xFFC00000-
    // 0xFFFFFFFF) represent the whole page directory. To access the first page
    // table, we just have to use the address 0xFFC00000 as explained above:
    // 0xFFC00000 = 0b 1111111111 0000000000 000000000000
    //                 dir_idx    tab_idx    page_offset, so 0xFFC00000 refers
    // to the last directory entry (the page directory) and then the first page
    // table inside the page directory which points to the physical location of
    // the page table. In other words, to access arbitrary parts of the page
    // directory after we enabled paging, we need to use following pointers:
    // 0xFFC00000 - the first page table (handles 0x00000000-0x003FFFFF)
    // 0xFFC01000 - the second page table (handles 0x00400000-0x007FFFFF)
    // ... (formula: 0xFFC00000 + tab_idx * PAGE_SIZE)
    // 0xFFFFE000 - the last "regular" page table (0xFF800000-0xFFBFFFFF)
    // 0xFFFFF000 - the page directory
    // In this way, we can map addresses as we like even with paging enabled.    
    vmm_dump();
    println("%2aok%a.");
}

void vmm_deinit() {
    // get the page directory's physical address by looking it up in itself
    page_directory_t* page_directory = vmm_get_physical_address(VMM_PAGEDIR);
    mmu_enable_paging(0); // then we can disable paging and free the directory
    vmm_set_page_directory(page_directory);
    vmm_destroy_page_directory();
    vmm_set_page_directory(0);
}
